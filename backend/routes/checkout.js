const express = require("express");
const Stripe = require("stripe");
const cartRouter = require("./cart");

const router = express.Router();

function getStripe() {
  if (!process.env.STRIPE_SECRET_KEY) {
    return null;
  }
  return Stripe(process.env.STRIPE_SECRET_KEY);
}

function getClientUrl(req) {
  return (
    process.env.CLIENT_URL ||
    req.get("origin") ||
    `http://${req.hostname}:5173`
  ).replace(/\/$/, "");
}

// POST /api/checkout/session
// Creates a hosted Stripe Checkout Session for the current cart.
router.post("/session", async (req, res) => {
  const { cartId = "demo" } = req.body;
  const stripe = getStripe();

  if (!stripe) {
    return res.status(500).json({
      error: "Missing STRIPE_SECRET_KEY in backend .env",
    });
  }

  const cart = cartRouter.getCart(cartId);
  if (!cart.items.length || cart.total <= 0) {
    return res.status(400).json({ error: "Cart is empty" });
  }

  const blockReason = cartRouter.getCheckoutBlockReason(cartId);
  if (blockReason) {
    return res.status(409).json({ error: blockReason });
  }

  try {
    const clientUrl = getClientUrl(req);
    const session = await stripe.checkout.sessions.create({
      mode: "payment",
      line_items: cart.items.map((item) => ({
        quantity: item.qty,
        price_data: {
          currency: "usd",
          unit_amount: Math.round(Number(item.price) * 100),
          product_data: {
            name: item.name,
            metadata: {
              barcode: item.barcode,
              aisle: item.aisle || "",
            },
          },
        },
      })),
      metadata: { cartId },
      success_url: `${clientUrl}/checkout-success?session_id={CHECKOUT_SESSION_ID}`,
      cancel_url: `${clientUrl}/cart?checkout=cancelled`,
    });

    res.json({
      mode: "stripe",
      url: session.url,
      sessionId: session.id,
    });
  } catch (error) {
    console.error("[Stripe] Could not create checkout session:", error);
    res.status(500).json({ error: "Could not create Stripe checkout session" });
  }
});

// POST /api/checkout/complete
// Verifies the Stripe session after redirect and clears the matching cart.
router.post("/complete", async (req, res) => {
  const { sessionId } = req.body;
  const stripe = getStripe();

  if (!stripe) {
    return res.status(500).json({
      error: "Missing STRIPE_SECRET_KEY in backend .env",
    });
  }

  if (!sessionId) {
    return res.status(400).json({ error: "Missing sessionId" });
  }

  try {
    const session = await stripe.checkout.sessions.retrieve(sessionId);
    if (session.payment_status !== "paid") {
      return res.status(400).json({
        success: false,
        message: "Stripe payment is not complete yet",
        paymentStatus: session.payment_status,
      });
    }

    const cartId = session.metadata?.cartId || "demo";
    const result = cartRouter.recordStripePayment(cartId, session);
    res.json(result);
  } catch (error) {
    console.error("[Stripe] Could not complete checkout:", error);
    res.status(500).json({ error: "Could not verify Stripe checkout session" });
  }
});

module.exports = router;
