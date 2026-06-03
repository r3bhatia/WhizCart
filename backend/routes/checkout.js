const express = require("express");
const router = express.Router();

// POST /api/checkout/session
// Returns a local checkout URL for demo/testing. The mock page completes payment
// by calling the existing /api/cart/checkout route.
router.post("/session", (req, res) => {
  const {
    cartId = "demo",
    total = 0,
    items = 0,
    method = "card",
  } = req.body;

  if (Number(total) <= 0 || Number(items) <= 0) {
    return res.status(400).json({ error: "Cart is empty" });
  }

  const origin = req.get("origin") || `http://${req.hostname}:5173`;
  const params = new URLSearchParams({
    cartId,
    total: String(total),
    items: String(items),
    method,
  });

  res.json({
    mode: "mock",
    url: `${origin}/mock-checkout?${params.toString()}`,
  });
});

module.exports = router;
