import { Link, useSearchParams } from "react-router-dom";

export default function MockCheckoutPage() {
  const [params] = useSearchParams();
  const total = Number(params.get("total") || 0);
  const itemCount = Number(params.get("items") || 0);
  const cartId = params.get("cartId") || "demo";

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

        <button className="btn btn-primary mock-pay-button">
          Pay ${total.toFixed(2)}
        </button>

        <p className="checkout-status">
          This is a local mock page. Start the backend with a Stripe test key to use real Stripe Checkout.
        </p>
      </div>

      <Link className="back-link" to="/cart">Back to cart</Link>
    </div>
  );
}
