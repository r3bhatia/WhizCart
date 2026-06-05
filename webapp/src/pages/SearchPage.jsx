import { useState, useEffect, useRef } from "react";
import ProductCard from "../components/ProductCard";
import { useCartSession } from "../utils/cartSession";
import { categoryLabel } from "../utils/productVisuals";

const CATEGORIES = ["all", "dairy", "bakery", "meat", "produce", "pantry", "snacks", "beverages", "frozen"];

export default function SearchPage() {
  const [query, setQuery] = useState("");
  const [results, setResults] = useState([]);
  const [loading, setLoading] = useState(false);
  const [addingBarcode, setAddingBarcode] = useState("");
  const [selectedCategory, setSelectedCategory] = useState("all");
  const debounceRef = useRef(null);
  const { cartId } = useCartSession();

  useEffect(() => {
    clearTimeout(debounceRef.current);
    debounceRef.current = setTimeout(async () => {
      setLoading(true);
      try {
        const params = new URLSearchParams();
        if (query.trim()) params.set("q", query.trim());
        if (selectedCategory !== "all") params.set("category", selectedCategory);
        const res = await fetch(`/api/items?${params.toString()}`);
        setResults(await res.json());
      } catch {
        setResults([]);
      }
      setLoading(false);
    }, 300);

    return () => clearTimeout(debounceRef.current);
  }, [query, selectedCategory]);

  async function addToCart(product) {
    setAddingBarcode(product.barcode);
    try {
      await fetch("/api/cart/scan", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ barcode: product.barcode, cartId }),
      });
    } finally {
      setAddingBarcode("");
    }
  }

  return (
    <div>
      <div className="grocery-hero">
        <p className="section-title">Fresh picks</p>
        <h1>What are you shopping for?</h1>
        <p>Search groceries, browse by aisle, and add items to your WhizCart.</p>
      </div>

      <input
        className="search-input"
        placeholder="Search products (e.g. milk, chips, eggs...)"
        value={query}
        onChange={e => setQuery(e.target.value)}
        autoFocus
      />

      <div className="category-strip" aria-label="Browse categories">
        {CATEGORIES.map(category => (
          <button
            key={category}
            className={`category-chip ${selectedCategory === category ? "active" : ""}`}
            onClick={() => setSelectedCategory(category)}
          >
            {categoryLabel(category)}
          </button>
        ))}
      </div>

      {loading && <p className="empty">Searching...</p>}

      {!loading && results.length === 0 && (
        <p className="empty">No groceries found.</p>
      )}

      <div className="product-list">
        {results.map(product => (
          <ProductCard
            key={product.barcode}
            product={product}
            onAdd={addToCart}
            adding={addingBarcode === product.barcode}
          />
        ))}
      </div>
    </div>
  );
}
