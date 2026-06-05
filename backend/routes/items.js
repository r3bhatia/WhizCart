// routes/items.js
const express = require("express");
const router = express.Router();
const { findByBarcode, getAllProducts } = require("../mock_db/products");

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
  const q = String(req.query.q || "").trim().toLowerCase();
  const category = String(req.query.category || "").trim().toLowerCase();

  let products = getAllProducts();
  if (category && category !== "all") {
    products = products.filter(p => p.category === category);
  }
  if (q) {
    products = products.filter(p =>
      p.name.toLowerCase().includes(q) ||
      p.category.toLowerCase().includes(q) ||
      (p.tags || []).some(tag => tag.toLowerCase().includes(q))
    );
  }

  res.json(products);
});

module.exports = router;
