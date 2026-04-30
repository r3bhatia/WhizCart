// webapp/src/components/ProductCard.jsx
export default function ProductCard({ product }) {
  return (
    <div className="card" style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start" }}>
      <div style={{ flex: 1 }}>
        <p style={{ fontWeight: 600, marginBottom: 6 }}>{product.name}</p>
        <div style={{ display: "flex", gap: 8, flexWrap: "wrap" }}>
          <span className="tag">{product.category}</span>
          <span className="aisle">📍 Aisle {product.aisle}</span>
        </div>
      </div>
      <div style={{ textAlign: "right", paddingLeft: 12 }}>
        <p className="price" style={{ fontSize: "1.1rem" }}>${product.price.toFixed(2)}</p>
      </div>
    </div>
  );
}
