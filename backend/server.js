// server.js
const express = require("express");
const cors = require("cors");
const app = express();
const PORT = process.env.PORT || 3001;

app.use(cors());
app.use(express.json());

// ── Routes ──────────────────────────────────────────────────────────────────
const itemsRouter = require("./routes/items");
const cartRouter  = require("./routes/cart");
const recsRouter  = require("./routes/recommendations");

// Expose the in-memory cart store globally so recommendations can read it
const cartModule = require("./routes/cart");
// (cart.js exports nothing extra; carts object lives inside that module)

app.use("/api/items", itemsRouter);
app.use("/api/cart",  cartRouter);
app.use("/api/recommendations", recsRouter);

// Health check
app.get("/", (req, res) => res.json({ status: "WhizCart backend running" }));

app.listen(PORT, () => {
  console.log(`✅ WhizCart backend listening on http://localhost:${PORT}`);
});
