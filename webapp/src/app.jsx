// webapp/src/App.jsx
import { BrowserRouter, Routes, Route, Link } from "react-router-dom";
import SearchPage from "./pages/SearchPage";
import CartPage   from "./pages/CartPage";
import MockCheckoutPage from "./pages/MockCheckoutPage";
import "./index.css";

export default function App() {
  return (
    <BrowserRouter>
      <div className="app">
        <nav className="navbar">
          <span className="logo">🛒 WhizCart</span>
          <div className="nav-links">
            <Link to="/">Search</Link>
            <Link to="/cart">My Cart</Link>
          </div>
        </nav>
        <main>
          <Routes>
            <Route path="/"     element={<SearchPage />} />
            <Route path="/cart" element={<CartPage />} />
            <Route path="/checkout/mock" element={<MockCheckoutPage />} />
          </Routes>
        </main>
      </div>
    </BrowserRouter>
  );
}
