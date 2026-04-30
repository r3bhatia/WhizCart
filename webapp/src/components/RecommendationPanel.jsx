// webapp/src/components/RecommendationPanel.jsx
export default function RecommendationPanel({ recommendations }) {
  if (!recommendations || recommendations.length === 0) return null;

  return (
    <div style={{
      background: "linear-gradient(135deg, rgba(108,99,255,0.08), rgba(0,212,170,0.06))",
      border: "1px solid rgba(108,99,255,0.3)",
      borderRadius: 14,
      padding: 16,
    }}>
      {recommendations.map(rec => (
        <div key={rec.barcode} style={{
          display: "flex",
          justifyContent: "space-between",
          alignItems: "center",
          padding: "10px 0",
          borderBottom: "1px solid rgba(255,255,255,0.06)",
        }}>
          <div>
            <p style={{ fontWeight: 500, fontSize: "0.95rem", marginBottom: 3 }}>{rec.name}</p>
            <span className="aisle">📍 Aisle {rec.aisle}</span>
          </div>
          <p className="price">${rec.price.toFixed(2)}</p>
        </div>
      ))}
    </div>
  );
}
