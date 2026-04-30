// routes/cart.js
// Simple in-memory cart (one cart per demo session).
// For multi-cart support, key by cartId from query param.

const express = require("express");
const router = express.Router();
const { findByBarcode } = require("../mock_db/products");

// In-memory store: { [cartId]: { items: [...], total: number } }
const carts = {};
const lastWeightStatus = {}; // { [cartId]: weightVerifyResult }

function getCart(cartId) {
  if (!carts[cartId]) carts[cartId] = { items: [], total: 0 };
  return carts[cartId];
}

// GET /api/cart?cartId=demo
router.get("/", (req, res) => {
  const cart = getCart(req.query.cartId || "demo");
  res.json(cart);
});

// POST /api/cart/scan  { barcode, cartId }
// Called by ESP32 after a successful scan
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
  cart.total = cart.items.reduce((sum, i) => sum + i.price * i.qty, 0);
  cart.total = Math.round(cart.total * 100) / 100; // 2 decimal places

  res.json({ product, cart });
});

// DELETE /api/cart/item  { barcode, cartId }
router.delete("/item", (req, res) => {
  const { barcode, cartId = "demo" } = req.body;
  const cart = getCart(cartId);
  cart.items = cart.items.filter(i => i.barcode !== barcode);
  cart.total = cart.items.reduce((sum, i) => sum + i.price * i.qty, 0);
  cart.total = Math.round(cart.total * 100) / 100;
  res.json(cart);
});

// DELETE /api/cart?cartId=demo  (clear entire cart)
router.delete("/", (req, res) => {
  const cartId = req.query.cartId || "demo";
  carts[cartId] = { items: [], total: 0 };
  res.json({ message: "Cart cleared" });
});

// POST /api/cart/verify-weight  { measuredG, itemCount, cartId }
// ESP32 sends the raw scale reading; backend computes expected weight
// from scanned items and returns a match/mismatch verdict.
router.post("/verify-weight", (req, res) => {
  const { measuredG, itemCount, cartId = "demo" } = req.body;
  const cart = getCart(cartId);

  // Sum expected weight from all scanned items × qty
  const expectedG = cart.items.reduce((sum, item) => {
    return sum + (item.weightG || 0) * item.qty;
  }, 0);

  const TOLERANCE_G = 100; // ±100g — adjust based on your scale accuracy
  const diffG = measuredG - expectedG;
  const ok = Math.abs(diffG) <= TOLERANCE_G;

  // Optionally flag if item count seems off (rough heuristic)
  const totalQty = cart.items.reduce((s, i) => s + i.qty, 0);
  const countMismatch = itemCount !== totalQty;

  const result = {
    ok: ok && !countMismatch,
    expectedG: Math.round(expectedG),
    measuredG: Math.round(measuredG),
    diffG: Math.round(diffG),
    expectedCount: totalQty,
    measuredCount: itemCount,
    message: ok
      ? (countMismatch ? "Weight OK but item count differs" : "All good!")
      : `Weight off by ${Math.round(diffG)}g — item may be unscanned`,
  };

  // Cache so webapp can poll it
  lastWeightStatus[cartId] = result;

  res.json(result);
});

// GET /api/cart/weight-status?cartId=demo
// Webapp polls this to display the latest weight verification result
router.get("/weight-status", (req, res) => {
  const cartId = req.query.cartId || "demo";
  const status = lastWeightStatus[cartId];
  if (!status) return res.status(404).json({ error: "No weight reading yet" });
  res.json(status);
});

module.exports = router;
