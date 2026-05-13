// routes/checkout.js
// Creates a mock checkout URL by default, or a real Stripe Checkout Session
// when STRIPE_SECRET_KEY is set to a Stripe test secret key.

const express = require("express");
const https = require("https");
const router = express.Router();

const { getCart } = require("./cart");

const DEFAULT_CART_ID = "demo";

function getClientBaseUrl(req) {
  const configuredUrl = process.env.CLIENT_URL || process.env.APP_URL;
  if (configuredUrl) return configuredUrl.replace(/\/$/, "");

  const origin = req.get("origin");
  if (origin) return origin.replace(/\/$/, "");

  return `${req.protocol}://${req.get("host")}`;
}

function buildMockCheckoutUrl(baseUrl, cartId, cart) {
  const params = new URLSearchParams({
    cartId,
    total: cart.total.toFixed(2),
    items: String(cart.items.reduce((sum, item) => sum + item.qty, 0)),
  });

  return `${baseUrl}/checkout/mock?${params.toString()}`;
}

function buildStripeForm(cart, successUrl, cancelUrl) {
  const form = new URLSearchParams({
    mode: "payment",
    success_url: successUrl,
    cancel_url: cancelUrl,
  });

  cart.items.forEach((item, index) => {
    const unitAmount = Math.round(Number(item.price || 0) * 100);
    form.append(`line_items[${index}][quantity]`, String(item.qty));
    form.append(`line_items[${index}][price_data][currency]`, "usd");
    form.append(`line_items[${index}][price_data][unit_amount]`, String(unitAmount));
    form.append(`line_items[${index}][price_data][product_data][name]`, item.name);
    form.append(`line_items[${index}][price_data][product_data][metadata][barcode]`, item.barcode);
  });

  return form;
}

function createStripeCheckoutSession(form) {
  const secretKey = process.env.STRIPE_SECRET_KEY;

  return new Promise((resolve, reject) => {
    const request = https.request({
      method: "POST",
      hostname: "api.stripe.com",
      path: "/v1/checkout/sessions",
      headers: {
        Authorization: `Bearer ${secretKey}`,
        "Content-Type": "application/x-www-form-urlencoded",
        "Content-Length": Buffer.byteLength(form.toString()),
      },
    }, (response) => {
      let body = "";
      response.on("data", chunk => { body += chunk; });
      response.on("end", () => {
        let parsed;
        try {
          parsed = JSON.parse(body);
        } catch {
          return reject(new Error("Stripe returned a non-JSON response"));
        }

        if (response.statusCode < 200 || response.statusCode >= 300) {
          const message = parsed.error?.message || "Stripe Checkout Session failed";
          const error = new Error(message);
          error.statusCode = response.statusCode;
          return reject(error);
        }

        resolve(parsed);
      });
    });

    request.on("error", reject);
    request.write(form.toString());
    request.end();
  });
}

// POST /api/checkout/session  { cartId }
router.post("/session", async (req, res) => {
  const cartId = req.body.cartId || DEFAULT_CART_ID;
  const cart = getCart(cartId);

  if (cart.items.length === 0) {
    return res.status(400).json({ error: "Cart is empty" });
  }

  const baseUrl = getClientBaseUrl(req);
  const successUrl = `${baseUrl}/cart?checkout=success`;
  const cancelUrl = `${baseUrl}/cart?checkout=cancelled`;

  if (!process.env.STRIPE_SECRET_KEY) {
    return res.json({
      mode: "mock",
      url: buildMockCheckoutUrl(baseUrl, cartId, cart),
      total: cart.total,
    });
  }

  if (!process.env.STRIPE_SECRET_KEY.startsWith("sk_test_")) {
    return res.status(400).json({
      error: "Use a Stripe test secret key that starts with sk_test_ for this demo checkout.",
    });
  }

  try {
    const session = await createStripeCheckoutSession(
      buildStripeForm(cart, successUrl, cancelUrl)
    );

    res.json({
      mode: "stripe_test",
      id: session.id,
      url: session.url,
      total: cart.total,
    });
  } catch (error) {
    res.status(error.statusCode || 502).json({ error: error.message });
  }
});

module.exports = router;
