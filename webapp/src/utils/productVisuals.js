const CATEGORY_STYLES = {
  dairy: { bg: "#e0f2fe", fg: "#075985", accent: "#bae6fd", label: "DAIRY" },
  bakery: { bg: "#fef3c7", fg: "#92400e", accent: "#fde68a", label: "BAKERY" },
  meat: { bg: "#fee2e2", fg: "#991b1b", accent: "#fecaca", label: "MEAT" },
  produce: { bg: "#dcfce7", fg: "#166534", accent: "#bbf7d0", label: "FRESH" },
  pantry: { bg: "#ede9fe", fg: "#5b21b6", accent: "#ddd6fe", label: "PANTRY" },
  snacks: { bg: "#ffedd5", fg: "#9a3412", accent: "#fed7aa", label: "SNACK" },
  beverages: { bg: "#ccfbf1", fg: "#115e59", accent: "#99f6e4", label: "DRINK" },
  frozen: { bg: "#e0e7ff", fg: "#3730a3", accent: "#c7d2fe", label: "FROZEN" },
};

function shortName(product) {
  const name = product?.name || "Grocery Item";
  return name
    .replace(/\([^)]*\)/g, "")
    .trim()
    .split(/\s+/)
    .slice(0, 2)
    .join(" ");
}

export function productImageUrl(product, size = 220) {
  const style = CATEGORY_STYLES[product?.category] || {
    bg: "#f1f5f9",
    fg: "#334155",
    accent: "#e2e8f0",
    label: "ITEM",
  };
  const name = shortName(product);

  const svg = `
    <svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" viewBox="0 0 220 220">
      <rect width="220" height="220" rx="24" fill="${style.bg}"/>
      <circle cx="172" cy="42" r="34" fill="${style.accent}"/>
      <circle cx="50" cy="176" r="42" fill="${style.accent}"/>
      <rect x="42" y="38" width="136" height="116" rx="18" fill="#fff" opacity="0.78"/>
      <rect x="62" y="58" width="96" height="18" rx="9" fill="${style.accent}"/>
      <text x="110" y="108" text-anchor="middle" font-family="Arial, sans-serif" font-size="20" font-weight="700" fill="${style.fg}">${style.label}</text>
      <text x="110" y="136" text-anchor="middle" font-family="Arial, sans-serif" font-size="14" font-weight="700" fill="${style.fg}">${name}</text>
    </svg>
  `;

  return `data:image/svg+xml;charset=UTF-8,${encodeURIComponent(svg)}`;
}

export function categoryLabel(category) {
  const labels = {
    all: "All",
    dairy: "Dairy",
    bakery: "Bakery",
    meat: "Meats",
    produce: "Produce",
    pantry: "Pantry",
    snacks: "Snacks",
    beverages: "Drinks",
    frozen: "Frozen",
  };
  return labels[category] || category;
}
