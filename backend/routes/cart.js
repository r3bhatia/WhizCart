// routes/cart.js
// Simple in-memory cart for one demo session.

const express = require("express");
const router = express.Router();
const { findByBarcode } = require("../mock_db/products");

const carts = {};
const lastWeightStatus = {};

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

// GET /api/cart?cartId=demo
router.get("/", (req, res) => {
  const cart = getCart(req.query.cartId || "demo");
  res.json(cart);
});

// POST /api/cart/scan  { barcode, cartId }
router.post("/scan", (req, res) => {
  const { barcode, cartId = "demo" } = req.body;
  const product = findByBarcode(barcode);

  if (!product) {
    return res.status(404).json({ error: "Product not found", barcode });
  }

  const cart = getCart(cartId);
  const existing = cart.items.find(i => i.barcode === barcode);

  if (existing) {
    existing.qty += 1;
  } else {
    cart.items.push({ ...product, qty: 1 });
  }

  recalculateTotal(cart);
  res.json({ product, cart });
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
  res.json({ message: "Cart cleared" });
});

// POST /api/cart/verify-weight  { measuredG, itemCount, cartId, autoRemove }
router.post("/verify-weight", (req, res) => {
  const { measuredG, itemCount, cartId = "demo", autoRemove = false } = req.body;
  const cart = getCart(cartId);

  const TOLERANCE_G = 25;
  const REMOVE_MATCH_TOLERANCE_G = 45;
  let removedItem = null;

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
    ok: ok && !countMismatch,
    expectedG: Math.round(expectedG),
    measuredG: Math.round(measuredG),
    diffG: Math.round(diffG),
    expectedCount: totalQty,
    measuredCount: itemCount,
    removedItem: removedItem
      ? { barcode: removedItem.barcode, name: removedItem.name, weightG: removedItem.weightG }
      : null,
    cartChanged: Boolean(removedItem),
    total: cart.total,
    message: removedItem
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

module.exports = router;
