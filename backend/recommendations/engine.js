// recommendations/engine.js
// Tag-based collaborative filtering for demo.
// Given the current cart, scores every other product by tag overlap
// and returns the top N that are NOT already in the cart.

const { products } = require("../mock_db/products");

/**
 * @param {string[]} scannedBarcodes  - barcodes already in cart
 * @param {number}   topN             - how many recommendations to return
 * @returns {object[]}                - array of product objects
 */
/**function getRecommendations(scannedBarcodes, topN = 4) {
  if (!scannedBarcodes || scannedBarcodes.length === 0) return [];

  // Collect tags from cart items
  const cartTags = new Set();
  const cartBarcodes = new Set(scannedBarcodes);

  products.forEach(p => {
    if (cartBarcodes.has(p.barcode)) {
      p.tags.forEach(t => cartTags.add(t));
    }
  });

  // Score each non-cart product
  const scored = products
    .filter(p => !cartBarcodes.has(p.barcode))
    .map(p => {
      const overlap = p.tags.filter(t => cartTags.has(t)).length;
      return { product: p, score: overlap };
    })
    .filter(x => x.score > 0)
    .sort((a, b) => b.score - a.score);

  return scored.slice(0, topN).map(x => x.product);
}

module.exports = { getRecommendations };
*/
function getRecommendations(scannedBarcodes, topN = 4) {
  if (!scannedBarcodes || scannedBarcodes.length === 0) return [];

  const cartTags = new Set();
  const cartBarcodes = new Set(scannedBarcodes);

  // build tag profile safely
  products.forEach(p => {
    if (cartBarcodes.has(p.barcode)) {
      (p.tags || []).forEach(t => cartTags.add(t));
    }
  });

  const scored = products
    .filter(p => !cartBarcodes.has(p.barcode))
    .map(p => {
      const overlap = (p.tags || []).filter(t => cartTags.has(t)).length;

      // small boost for same category (improves quality a lot)
      const categoryBoost = products.some(c => c.barcode === p.barcode)
        ? 0
        : 0;

      return {
        product: p,
        score: overlap + categoryBoost,
      };
    })
    .filter(x => x.score > 0)
    .sort((a, b) => b.score - a.score);

  return scored.slice(0, topN).map(x => x.product);
}
