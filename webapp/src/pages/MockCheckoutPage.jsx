import { Link, useSearchParams } from "react-router-dom";
import { useState } from "react";

export default function MockCheckoutPage() {
  const [params] = useSearchParams();
  const total = Number(params.get("total") || 0);
  const itemCount = Number(params.get("items") || 0);
  const cartId = params.get("cartId") || "demo";
  const method = params.get("method") || "card";
  const [status, setStatus] = useState("");
  const [paying, setPaying] = useState(false);

  async function pay() {
    setPaying(true);
    setStatus("");
    try {
      const res = await fetch("/api/cart/checkout", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ cartId, method }),
      });
      const data = await res.json();
      setStatus(data.message || "Payment complete");
    } catch {
      setStatus("Could not reach checkout");
    } finally {
      setPaying(false);
    }
  }

  return (
    <div className="mock-checkout">
      <p className="section-title">Stripe Test Checkout</p>

      <div className="card mock-checkout-panel">
        <div>
          <p className="mock-checkout-brand">WhizCart</p>
          <p className="aisle">Mock checkout session for cart {cartId}</p>
        </div>

        <div className="mock-checkout-summary">
          <span>{itemCount} item{itemCount !== 1 ? "s" : ""}</span>
          <strong>${total.toFixed(2)}</strong>
        </div>

        <button className="btn btn-primary mock-pay-button" disabled={paying || total <= 0} onClick={pay}>
          {paying ? "Processing..." : `Pay $${total.toFixed(2)}`}
        </button>

        {status && <p className="checkout-status">{status}</p>}
        {!status && (
          <p className="checkout-status">
            This is a local mock checkout page for demo payment testing.
          </p>
        )}
      </div>

      <Link className="back-link" to="/cart">Back to cart</Link>
    </div>
  );
}
