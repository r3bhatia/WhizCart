// routes/items.js
const express = require("express");
const router = express.Router();
const { findByBarcode, searchByName } = require("../mock_db/products");

// GET /api/items/:barcode
// Called by ESP32 after scanning a barcode
router.get("/:barcode", (req, res) => {
  const product = findByBarcode(req.params.barcode);
  if (!product) {
    return res.status(404).json({ error: "Product not found", barcode: req.params.barcode });
  }
  res.json(product);
});

// GET /api/items?q=peanut
// Called by webapp search
router.get("/", (req, res) => {
  const q = req.query.q || "";
  if (!q) return res.json([]);
  res.json(searchByName(q));
});

module.exports = router;
