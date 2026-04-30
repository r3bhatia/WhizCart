// webapp/src/pages/CartPage.jsx
import { useState, useEffect } from "react";
import RecommendationPanel from "../components/RecommendationPanel";

const CART_ID = "demo";

export default function CartPage() {
  const [cart,         setCart]         = useState({ items: [], total: 0 });
  const [recs,         setRecs]         = useState([]);
  const [weightStatus, setWeightStatus] = useState(null);
  const [deletingId,   setDeletingId]   = useState(null); // barcode being deleted

  async function fetchCart() {
    const res  = await fetch(`/api/cart?cartId=${CART_ID}`);
    const data = await res.json();
    setCart(data);
  }

  async function fetchRecs() {
    const res = await fetch(`/api/recommendations?cartId=${CART_ID}&n=4`);
    setRecs(await res.json());
  }

  async function fetchWeightStatus() {
    try {
      const res = await fetch(`/api/cart/weight-status?cartId=${CART_ID}`);
      if (res.ok) setWeightStatus(await res.json());
    } catch { /* scale may not be connected */ }
  }

  useEffect(() => {
    fetchCart(); fetchRecs(); fetchWeightStatus();
    const interval = setInterval(() => {
      fetchCart(); fetchRecs(); fetchWeightStatus();
    }, 5000);
    return () => clearInterval(interval);
  }, []);

  // ── Delete a single item ────────────────────────────────────────────────────
  async function deleteItem(barcode) {
    setDeletingId(barcode);
    try {
      await fetch("/api/cart/item", {
        method: "DELETE",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ barcode, cartId: CART_ID }),
      });
      await fetchCart();
      await fetchRecs();
    } finally {
      setDeletingId(null);
    }
  }

  // ── Clear entire cart ───────────────────────────────────────────────────────
  async function clearCart() {
    await fetch(`/api/cart?cartId=${CART_ID}`, { method: "DELETE" });
    setCart({ items: [], total: 0 });
    setRecs([]);
    setWeightStatus(null);
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
            <p style={{ fontWeight: 600, fontSize: "0.9rem", color: weightStatus.ok ? "#4ade80" : "#f87171" }}>
              {weightStatus.ok ? "Weight Verified" : "Weight Mismatch"}
            </p>
            <p style={{ fontSize: "0.78rem", color: "var(--muted)", marginTop: 2 }}>
              Expected {weightStatus.expectedG}g · Measured {weightStatus.measuredG}g · {weightStatus.message}
            </p>
          </div>
        </div>
      )}

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
            <div key={item.barcode} className="card" style={{
              display: "flex", justifyContent: "space-between",
              alignItems: "center", gap: 12,
            }}>
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
