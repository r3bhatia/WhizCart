// webapp/src/pages/CartPage.jsx
import { useState, useEffect } from "react";
import RecommendationPanel from "../components/RecommendationPanel";
import { useCartSession } from "../utils/cartSession";
import { productImageUrl } from "../utils/productVisuals";

export default function CartPage() {
  const [cart,         setCart]         = useState({ items: [], total: 0, pendingItem: null });
  const [recs,         setRecs]         = useState([]);
  const [weightStatus, setWeightStatus] = useState(null);
  const [deletingId,   setDeletingId]   = useState(null); // barcode being deleted
  const [paymentStatus, setPaymentStatus] = useState(null);
  const [checkoutSession, setCheckoutSession] = useState({
    loading: false,
    error: "",
    url: "",
    mode: "",
  });
  const { cartId } = useCartSession();

  async function fetchCart() {
    const res  = await fetch(`/api/cart?cartId=${encodeURIComponent(cartId)}`);
    const data = await res.json();
    setCart(data);
  }

  async function fetchRecs() {
    const res = await fetch(`/api/recommendations?cartId=${encodeURIComponent(cartId)}&n=4`);
    setRecs(await res.json());
  }

  async function fetchWeightStatus() {
    try {
      const res = await fetch(`/api/cart/weight-status?cartId=${encodeURIComponent(cartId)}`);
      if (res.ok) setWeightStatus(await res.json());
    } catch { /* scale may not be connected */ }
  }

  async function fetchPaymentStatus() {
    try {
      const res = await fetch(`/api/cart/payment-status?cartId=${encodeURIComponent(cartId)}`);
      if (res.ok) setPaymentStatus(await res.json());
    } catch { /* no payment yet */ }
  }

  useEffect(() => {
    fetchCart(); fetchRecs(); fetchWeightStatus(); fetchPaymentStatus();
    const interval = setInterval(() => {
      fetchCart(); fetchRecs(); fetchWeightStatus(); fetchPaymentStatus();
    }, 5000);
    return () => clearInterval(interval);
  }, [cartId]);

  // ── Delete a single item ────────────────────────────────────────────────────
  async function deleteItem(barcode) {
    setDeletingId(barcode);
    try {
      await fetch("/api/cart/item", {
        method: "DELETE",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ barcode, cartId }),
      });
      await fetchCart();
      await fetchRecs();
      await fetchWeightStatus();
    } finally {
      setDeletingId(null);
    }
  }

  async function createCheckoutUrl() {
    setCheckoutSession({ loading: true, error: "", url: "", mode: "" });

    try {
      const res = await fetch("/api/checkout/session", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ cartId }),
      });
      const data = await res.json();

      if (!res.ok) {
        throw new Error(data.error || "Could not create checkout URL");
      }

      setCheckoutSession({ loading: false, error: "", url: data.url, mode: data.mode });
      window.location.assign(data.url);
    } catch (error) {
      setCheckoutSession({ loading: false, error: error.message, url: "", mode: "" });
    }
  }

  // ── Clear entire cart ───────────────────────────────────────────────────────
  async function clearCart() {
    await fetch(`/api/cart?cartId=${encodeURIComponent(cartId)}`, { method: "DELETE" });
    setCart({ items: [], total: 0, pendingItem: null });
    setRecs([]);
    setWeightStatus(null);
    setPaymentStatus(null);
    setCheckoutSession({ loading: false, error: "", url: "", mode: "" });
  }

  const totalQty = cart.items.reduce((s, i) => s + i.qty, 0);

  return (
    <div>
      {/* Running total banner */}
      <div className="total-banner">
        <p className="label">Running Total</p>
        <p className="amount">${cart.total.toFixed(2)}</p>
        <p className="label" style={{ marginTop: 8 }}>
          {totalQty} item{totalQty !== 1 ? "s" : ""}
        </p>
      </div>

      {cart.pendingItem && (
        <div className="status-pending" role="status" aria-live="polite">
          <div>
            <p className="status-title">Waiting for Basket Weight</p>
            <p>
              {cart.pendingItem.name} is pending. Place it in the basket so WhizCart can confirm about{" "}
              {cart.pendingItem.weightG}g before adding it to checkout.
            </p>
          </div>
        </div>
      )}

      {/* Weight verification badge */}
      {weightStatus && (
        <div style={{
          display: "flex", alignItems: "center", gap: 12,
          padding: "12px 16px", borderRadius: 10, marginBottom: 20,
          background: weightStatus.ok ? "rgba(74,222,128,0.08)" : "rgba(248,113,113,0.08)",
          border: `1px solid ${weightStatus.ok ? "rgba(74,222,128,0.4)" : "rgba(248,113,113,0.4)"}`,
        }}>
          <span style={{ fontSize: "1.4rem" }}>{weightStatus.ok ? "✅" : "⚠️"}</span>
          <div>
            <p style={{ fontWeight: 600, fontSize: "0.9rem", color: weightStatus.ok ? "var(--accent-dark)" : "var(--danger)" }}>
              {weightStatus.event === "confirmed"
                ? "Item Confirmed"
                : weightStatus.event === "cancelled"
                  ? "Item Not Added"
                  : weightStatus.event === "pending"
                    ? "Waiting for Weight"
                    : weightStatus.ok
                      ? "Weight Verified"
                      : "Weight Mismatch"}
            </p>
            <p style={{ fontSize: "0.78rem", color: "var(--muted)", marginTop: 2 }}>
              Expected {weightStatus.expectedG}g · Measured {weightStatus.measuredG}g · {weightStatus.message}
            </p>
          </div>
        </div>
      )}

      {/* Payment */}
      <div className="card" style={{ marginBottom: 22 }}>
        <div style={{ display: "flex", justifyContent: "space-between", gap: 12, alignItems: "center", marginBottom: 14 }}>
          <div>
            <p className="section-title" style={{ marginBottom: 4 }}>Stripe Checkout</p>
            <p style={{ color: "var(--muted)", fontSize: "0.9rem" }}>
              Pay securely with card, Apple Pay, or other Stripe-supported methods.
            </p>
          </div>
          <p className="price" style={{ fontSize: "1.4rem" }}>${cart.total.toFixed(2)}</p>
        </div>

        <button
          className="btn btn-primary checkout-button"
          style={{ width: "100%", borderRadius: 10 }}
          disabled={checkoutSession.loading || cart.total <= 0}
          onClick={createCheckoutUrl}
        >
          {checkoutSession.loading ? "Creating Stripe checkout..." : `Go to Stripe Checkout - $${cart.total.toFixed(2)}`}
        </button>

        {checkoutSession.error && <p className="checkout-error">{checkoutSession.error}</p>}

        {paymentStatus && (
          <p style={{
            marginTop: 12,
            color: paymentStatus.success ? "var(--accent-dark)" : "var(--danger)",
            fontWeight: 700,
          }}>
            {paymentStatus.message}
          </p>
        )}
      </div>

      {/* Cart items */}
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 12 }}>
        <p className="section-title">Cart Items</p>
        {cart.items.length > 0 && (
          <button className="btn btn-danger" onClick={clearCart}>Clear All</button>
        )}
      </div>

      {cart.items.length === 0
        ? <p className="empty">No items yet — scan something!</p>
        : cart.items.map(item => (
            <div key={item.barcode} className="card cart-item-card">
              <img
                className="cart-item-image"
                src={productImageUrl(item, 180)}
                alt={item.name}
                loading="lazy"
              />

              {/* Item info */}
              <div style={{ flex: 1, minWidth: 0 }}>
                <p style={{ fontWeight: 600, marginBottom: 4, whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }}>
                  {item.name}
                </p>
                <div style={{ display: "flex", gap: 8, alignItems: "center" }}>
                  <span className="tag">{item.category}</span>
                  <span className="aisle">Aisle {item.aisle}</span>
                  {item.weightG && (
                    <span style={{ fontSize: "0.72rem", color: "var(--muted)" }}>{item.weightG}g</span>
                  )}
                </div>
              </div>

              {/* Price + qty */}
              <div style={{ textAlign: "right", flexShrink: 0 }}>
                <p className="price">${(item.price * item.qty).toFixed(2)}</p>
                {item.qty > 1 && (
                  <p className="aisle">${item.price.toFixed(2)} × {item.qty}</p>
                )}
              </div>

              {/* Delete button */}
              <button
                className="btn btn-danger"
                style={{ padding: "6px 12px", fontSize: "1rem", flexShrink: 0 }}
                disabled={deletingId === item.barcode}
                onClick={() => deleteItem(item.barcode)}
              >
                {deletingId === item.barcode ? "…" : "✕"}
              </button>
            </div>
          ))
      }

      {/* Recommendations */}
      {recs.length > 0 && (
        <>
          <p className="section-title" style={{ marginTop: 28 }}>You Might Also Want</p>
          <RecommendationPanel recommendations={recs} />
        </>
      )}
    </div>
  );
}
