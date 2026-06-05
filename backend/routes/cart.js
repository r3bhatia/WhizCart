// routes/cart.js
// Simple in-memory cart for one demo session.

const express = require("express");
const router = express.Router();
const { findByBarcode } = require("../mock_db/products");

const carts = {};
const lastWeightStatus = {};
const lastPaymentStatus = {};
const pendingScans = {};
const basketConnections = {};

const TOLERANCE_G = 25;
const REMOVE_MATCH_TOLERANCE_G = 45;
const PENDING_SCAN_TIMEOUT_MS = 15000;
const NO_CHANGE_TOLERANCE_G = 25;

function getCart(cartId) {
  if (!carts[cartId]) carts[cartId] = { items: [], total: 0 };
  return carts[cartId];
}

function recalculateTotal(cart) {
  cart.total = cart.items.reduce((sum, item) => sum + item.price * item.qty, 0);
  cart.total = Math.round(cart.total * 100) / 100;
}

function getExpectedWeight(cart) {
  return cart.items.reduce((sum, item) => sum + (item.weightG || 0) * item.qty, 0);
}

function getTotalQty(cart) {
  return cart.items.reduce((sum, item) => sum + item.qty, 0);
}

function addOneItem(cart, product) {
  const existing = cart.items.find(i => i.barcode === product.barcode);

  if (existing) {
    existing.qty += 1;
  } else {
    cart.items.push({ ...product, qty: 1 });
  }

  recalculateTotal(cart);
}

function removeOneItem(cart, barcode) {
  const item = cart.items.find(i => i.barcode === barcode);
  if (!item) return null;

  item.qty -= 1;
  if (item.qty <= 0) {
    cart.items = cart.items.filter(i => i.barcode !== barcode);
  }

  recalculateTotal(cart);
  return item;
}

function findClosestItemByWeight(cart, targetWeightG) {
  let best = null;
  let bestDiff = Infinity;

  for (const item of cart.items) {
    const weightG = item.weightG || 0;
    if (weightG <= 0 || item.qty <= 0) continue;

    const diff = Math.abs(weightG - targetWeightG);
    if (diff < bestDiff) {
      best = item;
      bestDiff = diff;
    }
  }

  return best ? { item: best, diffG: bestDiff } : null;
}

function getProductWeightTolerance(product) {
  const weightG = product.weightG || 0;
  const isLooseItem = product.category === "produce";
  const percentTolerance = isLooseItem ? 0.35 : 0.15;
  const minimumTolerance = isLooseItem ? 35 : TOLERANCE_G;
  return Math.max(minimumTolerance, weightG * percentTolerance);
}

function pendingScanPayload(pending) {
  if (!pending) return null;
  return {
    barcode: pending.product.barcode,
    name: pending.product.name,
    price: pending.product.price,
    weightG: pending.product.weightG,
    category: pending.product.category,
    scannedAt: pending.scannedAt,
    message: pending.message,
  };
}

function buildCartResponse(cartId) {
  const cart = getCart(cartId);
  return {
    ...cart,
    pendingItem: pendingScanPayload(pendingScans[cartId]),
  };
}

function getCheckoutBlockReason(cartId) {
  const cart = getCart(cartId);
  const pending = pendingScans[cartId];
  const status = lastWeightStatus[cartId];

  if (pending) {
    return `${pending.product.name} is waiting for basket weight confirmation`;
  }

  if (cart.items.length > 0 && !status) {
    return "Basket weight has not been verified yet";
  }

  if (cart.items.length > 0 && status && !status.ok) {
    return status.message || "Basket weight mismatch must be resolved before checkout";
  }

  return "";
}

// GET /api/cart?cartId=demo
router.get("/", (req, res) => {
  const cartId = req.query.cartId || "demo";
  res.json(buildCartResponse(cartId));
});

// POST /api/cart/connect  { cartId, connected }
router.post("/connect", (req, res) => {
  const { cartId = "demo", connected = true } = req.body;
  basketConnections[cartId] = {
    connected: Boolean(connected),
    updatedAt: new Date().toISOString(),
  };
  res.json({ cartId, ...basketConnections[cartId] });
});

// GET /api/cart/connection-status?cartId=demo
router.get("/connection-status", (req, res) => {
  const cartId = req.query.cartId || "demo";
  const status = basketConnections[cartId] || { connected: false, updatedAt: null };
  res.json({ cartId, ...status });
});

// POST /api/cart/scan  { barcode, cartId }
router.post("/scan", (req, res) => {
  const { barcode, cartId = "demo" } = req.body;
  const product = findByBarcode(barcode);

  if (!product) {
    return res.status(404).json({ error: "Product not found", barcode });
  }

  const cart = getCart(cartId);
  const existingPending = pendingScans[cartId];
  if (existingPending && Date.now() - existingPending.scannedAt <= PENDING_SCAN_TIMEOUT_MS) {
    return res.status(409).json({
      error: `Finish placing ${existingPending.product.name} before scanning another item`,
      pendingItem: pendingScanPayload(existingPending),
      cart: buildCartResponse(cartId),
    });
  }

  if (existingPending) {
    pendingScans[cartId] = null;
  }

  const measuredBaseline = lastWeightStatus[cartId]?.measuredG;
  const baselineG = Number.isFinite(measuredBaseline) ? measuredBaseline : getExpectedWeight(cart);

  pendingScans[cartId] = {
    product,
    baselineG,
    scannedAt: Date.now(),
    message: `Place ${product.name} in basket to confirm weight`,
  };

  lastWeightStatus[cartId] = {
    ok: false,
    expectedG: Math.round(getExpectedWeight(cart) + (product.weightG || 0)),
    measuredG: Math.round(baselineG),
    diffG: 0,
    expectedCount: getTotalQty(cart),
    measuredCount: null,
    removedItem: null,
    pendingItem: pendingScanPayload(pendingScans[cartId]),
    cartChanged: false,
    total: cart.total,
    event: "pending",
    message: pendingScans[cartId].message,
  };

  res.json({ product, cart: buildCartResponse(cartId), pending: true, message: pendingScans[cartId].message });
});

// DELETE /api/cart/item  { barcode, cartId }
router.delete("/item", (req, res) => {
  const { barcode, cartId = "demo" } = req.body;
  const cart = getCart(cartId);
  removeOneItem(cart, barcode);
  res.json(cart);
});

// DELETE /api/cart?cartId=demo
router.delete("/", (req, res) => {
  const cartId = req.query.cartId || "demo";
  carts[cartId] = { items: [], total: 0 };
  lastWeightStatus[cartId] = null;
  lastPaymentStatus[cartId] = null;
  pendingScans[cartId] = null;
  res.json({ message: "Cart cleared" });
});

// POST /api/cart/checkout  { cartId, method }
router.post("/checkout", (req, res) => {
  const { cartId = "demo", method = "card" } = req.body;
  const cart = getCart(cartId);
  const normalizedMethod = ["card", "tap", "cash"].includes(method) ? method : "card";
  const total = cart.total;
  const itemCount = getTotalQty(cart);

  if (itemCount === 0 || total <= 0) {
    const result = {
      success: false,
      method: normalizedMethod,
      total,
      itemCount,
      message: "Cart is empty",
    };
    lastPaymentStatus[cartId] = result;
    return res.status(400).json(result);
  }

  const blockReason = getCheckoutBlockReason(cartId);
  if (blockReason) {
    const result = {
      success: false,
      method: normalizedMethod,
      total,
      itemCount,
      message: blockReason,
    };
    lastPaymentStatus[cartId] = result;
    return res.status(409).json(result);
  }

  const result = {
    success: true,
    method: normalizedMethod,
    total,
    itemCount,
    paidAt: new Date().toISOString(),
    message: `Paid $${total.toFixed(2)} with ${normalizedMethod}`,
  };

  carts[cartId] = { items: [], total: 0 };
  lastWeightStatus[cartId] = null;
  lastPaymentStatus[cartId] = result;
  pendingScans[cartId] = null;
  res.json(result);
});

// GET /api/cart/payment-status?cartId=demo
router.get("/payment-status", (req, res) => {
  const cartId = req.query.cartId || "demo";
  const status = lastPaymentStatus[cartId];
  if (!status) return res.status(404).json({ error: "No payment yet" });
  res.json(status);
});

// POST /api/cart/verify-weight  { measuredG, itemCount, cartId, autoRemove }
router.post("/verify-weight", (req, res) => {
  const { measuredG, itemCount, cartId = "demo", autoRemove = false } = req.body;
  const cart = getCart(cartId);

  let removedItem = null;
  let confirmedItem = null;
  let rejectedItem = null;
  let event = "verified";
  let pending = pendingScans[cartId];

  if (pending) {
    const elapsedMs = Date.now() - pending.scannedAt;
    const product = pending.product;
    const expectedDeltaG = product.weightG || 0;
    const measuredDeltaG = measuredG - pending.baselineG;
    const deltaDiffG = measuredDeltaG - expectedDeltaG;
    const toleranceG = getProductWeightTolerance(product);

    if (Math.abs(deltaDiffG) <= toleranceG) {
      addOneItem(cart, product);
      confirmedItem = product;
      pendingScans[cartId] = null;
      pending = null;
      event = "confirmed";
    } else if (Math.abs(measuredDeltaG) <= NO_CHANGE_TOLERANCE_G && elapsedMs > PENDING_SCAN_TIMEOUT_MS) {
      rejectedItem = product;
      pendingScans[cartId] = null;
      pending = null;
      event = "cancelled";
    } else if (Math.abs(measuredDeltaG) > Math.max(toleranceG, TOLERANCE_G)) {
      rejectedItem = product;
      pendingScans[cartId] = null;
      pending = null;
      event = "mismatch";
    } else {
      const result = {
        ok: false,
        expectedG: Math.round(getExpectedWeight(cart) + expectedDeltaG),
        measuredG: Math.round(measuredG),
        diffG: Math.round(deltaDiffG),
        expectedCount: getTotalQty(cart),
        measuredCount: itemCount,
        removedItem: null,
        confirmedItem: null,
        rejectedItem: null,
        pendingItem: pendingScanPayload(pending),
        cartChanged: false,
        total: cart.total,
        event: "pending",
        message: `Place ${product.name} in basket to confirm weight`,
      };

      lastWeightStatus[cartId] = result;
      return res.json(result);
    }
  }

  let expectedG = getExpectedWeight(cart);
  let diffG = measuredG - expectedG;

  if (autoRemove && diffG < -TOLERANCE_G && cart.items.length > 0) {
    const missingWeightG = Math.abs(diffG);
    const match = findClosestItemByWeight(cart, missingWeightG);

    if (match && match.diffG <= REMOVE_MATCH_TOLERANCE_G) {
      removedItem = removeOneItem(cart, match.item.barcode);
      expectedG = getExpectedWeight(cart);
      diffG = measuredG - expectedG;
    }
  }

  const ok = Math.abs(diffG) <= TOLERANCE_G;
  const totalQty = getTotalQty(cart);
  const countMismatch = !removedItem && Number.isFinite(itemCount) && itemCount !== totalQty;

  const result = {
    ok: event === "mismatch" || event === "cancelled" ? false : ok && !countMismatch,
    expectedG: Math.round(expectedG),
    measuredG: Math.round(measuredG),
    diffG: Math.round(diffG),
    expectedCount: totalQty,
    measuredCount: itemCount,
    removedItem: removedItem
      ? { barcode: removedItem.barcode, name: removedItem.name, weightG: removedItem.weightG }
      : null,
    confirmedItem: confirmedItem
      ? { barcode: confirmedItem.barcode, name: confirmedItem.name, price: confirmedItem.price, weightG: confirmedItem.weightG }
      : null,
    rejectedItem: rejectedItem
      ? { barcode: rejectedItem.barcode, name: rejectedItem.name, weightG: rejectedItem.weightG }
      : null,
    pendingItem: pendingScanPayload(pendingScans[cartId]),
    cartChanged: Boolean(removedItem || confirmedItem),
    total: cart.total,
    event,
    message: confirmedItem
      ? `${confirmedItem.name} confirmed by weight`
      : rejectedItem && event === "mismatch"
        ? `Weight mismatch for ${rejectedItem.name}. Please scan again.`
        : rejectedItem && event === "cancelled"
          ? `${rejectedItem.name} was not added because weight did not change`
          : removedItem
            ? `Removed ${removedItem.name} based on weight change`
            : ok
              ? (countMismatch ? "Weight OK but item count differs" : "All good!")
              : diffG > 0
                ? `Weight is ${Math.round(diffG)}g higher than scanned items`
                : `Weight is ${Math.round(Math.abs(diffG))}g lower than scanned items`,
  };

  lastWeightStatus[cartId] = result;
  res.json(result);
});

// GET /api/cart/weight-status?cartId=demo
router.get("/weight-status", (req, res) => {
  const cartId = req.query.cartId || "demo";
  const status = lastWeightStatus[cartId];
  if (!status) return res.status(404).json({ error: "No weight reading yet" });
  res.json(status);
});

router.getCart = getCart;
router.getCheckoutBlockReason = getCheckoutBlockReason;
router.recordStripePayment = function recordStripePayment(cartId, session) {
  const cart = getCart(cartId);
  const total = cart.total;
  const itemCount = getTotalQty(cart);

  const result = {
    success: true,
    method: "stripe",
    total,
    itemCount,
    stripeSessionId: session.id,
    paidAt: new Date().toISOString(),
    message: `Paid $${total.toFixed(2)} with Stripe`,
  };

  carts[cartId] = { items: [], total: 0 };
  lastWeightStatus[cartId] = null;
  lastPaymentStatus[cartId] = result;
  pendingScans[cartId] = null;
  return result;
};

module.exports = router;
