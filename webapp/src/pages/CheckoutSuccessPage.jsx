import { Link, useSearchParams } from "react-router-dom";
import { useEffect, useState } from "react";

export default function CheckoutSuccessPage() {
  const [params] = useSearchParams();
  const sessionId = params.get("session_id");
  const [status, setStatus] = useState({
    loading: true,
    success: false,
    message: "Verifying Stripe payment...",
  });

  useEffect(() => {
    async function completeCheckout() {
      if (!sessionId) {
        setStatus({
          loading: false,
          success: false,
          message: "Missing Stripe session id.",
        });
        return;
      }

      try {
        const res = await fetch("/api/checkout/complete", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ sessionId }),
        });
        const data = await res.json();

        setStatus({
          loading: false,
          success: res.ok && data.success,
          message: data.message || data.error || "Stripe checkout complete.",
        });
      } catch {
        setStatus({
          loading: false,
          success: false,
          message: "Could not verify Stripe payment.",
        });
      }
    }

    completeCheckout();
  }, [sessionId]);

  return (
    <div className="checkout-page">
      <div className="card checkout-panel">
        <p className="section-title">Stripe Checkout</p>
        <p className={status.success ? "checkout-success-title" : "checkout-error-title"}>
          {status.loading ? "Checking payment..." : status.success ? "Payment complete" : "Payment needs attention"}
        </p>
        <p className="checkout-status">{status.message}</p>
        <Link className="back-link" to="/cart">Back to cart</Link>
      </div>
    </div>
  );
}
