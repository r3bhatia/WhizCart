// webapp/src/pages/SearchPage.jsx
import { useState, useEffect, useRef } from "react";
import ProductCard from "../components/ProductCard";

export default function SearchPage() {
  const [query,   setQuery]   = useState("");
  const [results, setResults] = useState([]);
  const [loading, setLoading] = useState(false);
  const [addingId, setAddingId] = useState("");
  const [notice, setNotice] = useState("");
  const debounceRef = useRef(null);
  const CART_ID = "demo";

  useEffect(() => {
    if (!query.trim()) { setResults([]); return; }
    clearTimeout(debounceRef.current);
    debounceRef.current = setTimeout(async () => {
      setLoading(true);
      try {
        const res = await fetch(`/api/items?q=${encodeURIComponent(query)}`);
        setResults(await res.json());
      } catch { setResults([]); }
      setLoading(false);
    }, 300);
  }, [query]);

  async function addToCart(product) {
    setAddingId(product.barcode);
    setNotice("");

    try {
      const res = await fetch("/api/cart/scan", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ barcode: product.barcode, cartId: CART_ID }),
      });
      const data = await res.json();

      if (!res.ok) {
        throw new Error(data.error || "Could not add item");
      }

      setNotice(`Added ${product.name} to your cart.`);
    } catch (error) {
      setNotice(error.message);
    } finally {
      setAddingId("");
    }
  }

  return (
    <div>
      <h1 style={{ fontSize: "1.4rem", fontWeight: 700, marginBottom: 18 }}>
        Find an Item
      </h1>
      <input
        className="search-input"
        placeholder="Search products (e.g. milk, chips, eggs…)"
        value={query}
        onChange={e => setQuery(e.target.value)}
        autoFocus
      />

      {notice && <p className="search-notice">{notice}</p>}

      {loading && <p className="empty">Searching…</p>}

      {!loading && results.length === 0 && query.length > 0 && (
        <p className="empty">No results for "{query}"</p>
      )}

      {results.map(p => (
        <ProductCard
          key={p.barcode}
          product={p}
          adding={addingId === p.barcode}
          onAdd={addToCart}
        />
      ))}
    </div>
  );
}
