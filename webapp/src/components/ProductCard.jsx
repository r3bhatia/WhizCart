// webapp/src/components/ProductCard.jsx
import { productImageUrl } from "../utils/productVisuals";

export default function ProductCard({ product, onAdd, adding }) {
  return (
    <div className="card product-card">
      <img
        className="product-image"
        src={productImageUrl(product)}
        alt={product.name}
        loading="lazy"
      />

      <div className="product-card-body">
        <p className="product-name">{product.name}</p>
        <div style={{ display: "flex", gap: 8, flexWrap: "wrap" }}>
          <span className="tag">{product.category}</span>
          <span className="aisle">Aisle {product.aisle}</span>
          {product.weightG && <span className="aisle">{product.weightG}g</span>}
        </div>
      </div>

      <div className="product-card-actions">
        <p className="price" style={{ fontSize: "1.1rem" }}>${product.price.toFixed(2)}</p>
        {onAdd && (
          <button
            className="btn btn-primary add-btn"
            disabled={adding}
            onClick={() => onAdd(product)}
          >
            {adding ? "Pending..." : "Verify in Basket"}
          </button>
        )}
      </div>
    </div>
  );
}
