// webapp/src/App.jsx
import { BrowserRouter, Routes, Route, Link } from "react-router-dom";
import SearchPage from "./pages/SearchPage";
import CartPage from "./pages/CartPage";
import MockCheckoutPage from "./pages/MockCheckoutPage";
import CheckoutSuccessPage from "./pages/CheckoutSuccessPage";
import BasketConnectionGate from "./components/BasketConnectionGate";
import { useCartSession } from "./utils/cartSession";
import "./index.css";

function AppShell() {
  const { cartId, isConfirmed, needsConfirmation, confirmCart, rejectCart, pathWithCart } = useCartSession();

  if (!cartId || needsConfirmation) {
    return (
      <div className="app">
        <nav className="navbar">
          <span className="logo">WhizCart</span>
        </nav>
        <main>
          <BasketConnectionGate
            cartId={cartId}
            onConfirm={confirmCart}
            onReject={rejectCart}
          />
        </main>
      </div>
    );
  }

  return (
    <div className="app">
      <nav className="navbar">
        <span className="logo">WhizCart</span>
        <div className="nav-links">
          {isConfirmed && <span className="basket-id">Basket {cartId}</span>}
          <Link to={pathWithCart("/")}>Search</Link>
          <Link to={pathWithCart("/cart")}>My Cart</Link>
        </div>
      </nav>
      <main>
        <Routes>
          <Route path="/" element={<SearchPage />} />
          <Route path="/cart" element={<CartPage />} />
          <Route path="/mock-checkout" element={<MockCheckoutPage />} />
          <Route path="/checkout-success" element={<CheckoutSuccessPage />} />
        </Routes>
      </main>
    </div>
  );
}

export default function App() {
  return (
    <BrowserRouter>
      <AppShell />
    </BrowserRouter>
  );
}
