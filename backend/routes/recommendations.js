// routes/recommendations.js
const express = require("express");
const router = express.Router();
const { getRecommendations } = require("../recommendations/engine");
const { carts } = require("./cart"); // reuse the in-memory cart

// GET /api/recommendations?cartId=demo&n=4
router.get("/", (req, res) => {
  const cartId = req.query.cartId || "demo";
  const n = parseInt(req.query.n) || 4;

  // Pull barcodes from the live cart
  const cart = (global._carts || {})[cartId] || { items: [] };
  const barcodes = cart.items.map(i => i.barcode);

  const recs = getRecommendations(barcodes, n);
  res.json(recs);
});

module.exports = router;
