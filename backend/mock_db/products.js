// mock_db/products.js
// Demo product catalog. Each entry has a real-ish UPC barcode.
// Print docs/demo_barcodes.pdf and stick on physical items for the demo.
// To expand: just add more objects to this array.

const products = [
  // ── Dairy ────────────────────────────────────────────────────────────────
  { barcode: "012345678901", name: "Whole Milk (1 gal)",    price: 3.99, aisle: "A3", category: "dairy",    weightG: 3900, tags: ["dairy", "breakfast", "drink"] },
  { barcode: "012345678902", name: "Cheddar Cheese (8oz)",  price: 4.49, aisle: "A3", category: "dairy",    weightG: 227,  tags: ["dairy", "snack", "sandwich"] },
  { barcode: "012345678903", name: "Greek Yogurt",          price: 1.99, aisle: "A3", category: "dairy",    weightG: 170,  tags: ["dairy", "breakfast", "healthy"] },
  { barcode: "012345678904", name: "Butter (1 lb)",         price: 3.49, aisle: "A3", category: "dairy",    weightG: 454,  tags: ["dairy", "baking", "cooking"] },

  // ── Bread & Bakery ───────────────────────────────────────────────────────
  { barcode: "012345678905", name: "White Sandwich Bread",  price: 2.99, aisle: "B1", category: "bakery",   weightG: 567,  tags: ["bread", "sandwich", "breakfast"] },
  { barcode: "012345678906", name: "Whole Wheat Bread",     price: 3.49, aisle: "B1", category: "bakery",   weightG: 567,  tags: ["bread", "sandwich", "healthy"] },
  { barcode: "012345678907", name: "Bagels (6-pack)",       price: 3.99, aisle: "B1", category: "bakery",   weightG: 480,  tags: ["bread", "breakfast", "dairy"] },

  // ── Meat & Protein ───────────────────────────────────────────────────────
  { barcode: "012345678908", name: "Chicken Breast (2 lb)", price: 7.99, aisle: "C2", category: "meat",     weightG: 907,  tags: ["meat", "protein", "dinner", "healthy"] },
  { barcode: "012345678909", name: "Ground Beef (1 lb)",    price: 5.49, aisle: "C2", category: "meat",     weightG: 454,  tags: ["meat", "protein", "dinner"] },
  { barcode: "012345678910", name: "Salmon Fillet (1 lb)",  price: 9.99, aisle: "C3", category: "meat",     weightG: 454,  tags: ["seafood", "protein", "dinner", "healthy"] },
  { barcode: "012345678911", name: "Eggs (dozen)",          price: 3.29, aisle: "A3", category: "dairy",    weightG: 680,  tags: ["breakfast", "protein", "baking"] },

  // ── Produce ─────────────────────────────────────────────────────────────
  { barcode: "012345678912", name: "Banana (bunch)",        price: 1.29, aisle: "D1", category: "produce",  weightG: 680,  tags: ["fruit", "snack", "healthy", "breakfast"] },
  { barcode: "012345678913", name: "Baby Spinach (5oz)",    price: 3.99, aisle: "D1", category: "produce",  weightG: 142,  tags: ["vegetable", "salad", "healthy"] },
  { barcode: "012345678914", name: "Roma Tomatoes (3-pack)",price: 2.49, aisle: "D1", category: "produce",  weightG: 340,  tags: ["vegetable", "salad", "sandwich"] },
  { barcode: "012345678915", name: "Avocado",               price: 1.49, aisle: "D1", category: "produce",  weightG: 200,  tags: ["fruit", "healthy", "sandwich", "snack"] },

  // ── Pantry / Dry Goods ───────────────────────────────────────────────────
  { barcode: "012345678916", name: "Pasta (16oz)",          price: 1.79, aisle: "E2", category: "pantry",   weightG: 454,  tags: ["pasta", "dinner", "cooking"] },
  { barcode: "012345678917", name: "Marinara Sauce (24oz)", price: 2.99, aisle: "E2", category: "pantry",   weightG: 680,  tags: ["pasta", "dinner", "cooking"] },
  { barcode: "012345678918", name: "Olive Oil (16oz)",      price: 6.99, aisle: "E2", category: "pantry",   weightG: 473,  tags: ["cooking", "healthy", "baking"] },
  { barcode: "012345678919", name: "Rice (5 lb bag)",       price: 4.99, aisle: "E3", category: "pantry",   weightG: 2268, tags: ["grain", "dinner", "cooking"] },
  { barcode: "012345678920", name: "Peanut Butter (16oz)",  price: 3.49, aisle: "E1", category: "pantry",   weightG: 454,  tags: ["snack", "sandwich", "breakfast", "protein"] },
  { barcode: "012345678921", name: "Strawberry Jam (18oz)", price: 2.99, aisle: "E1", category: "pantry",   weightG: 510,  tags: ["snack", "sandwich", "breakfast"] },

  // ── Snacks ───────────────────────────────────────────────────────────────
  { barcode: "012345678922", name: "Tortilla Chips (13oz)", price: 3.99, aisle: "F1", category: "snacks",   weightG: 369,  tags: ["snack", "party"] },
  { barcode: "012345678923", name: "Salsa (16oz)",          price: 2.99, aisle: "F1", category: "snacks",   weightG: 454,  tags: ["snack", "party", "condiment"] },
  { barcode: "012345678924", name: "Granola Bars (6-pack)", price: 4.49, aisle: "F2", category: "snacks",   weightG: 252,  tags: ["snack", "breakfast", "healthy"] },
  { barcode: "012345678925", name: "Dark Chocolate Bar",    price: 2.49, aisle: "F2", category: "snacks",   weightG: 100,  tags: ["snack", "dessert"] },

  // ── Beverages ────────────────────────────────────────────────────────────
  { barcode: "012345678926", name: "Orange Juice (52oz)",      price: 4.99, aisle: "G1", category: "beverages", weightG: 1560, tags: ["drink", "breakfast", "fruit"] },
  { barcode: "012345678927", name: "Sparkling Water (12-pack)", price: 5.99, aisle: "G2", category: "beverages", weightG: 4320, tags: ["drink", "healthy"] },
  { barcode: "012345678928", name: "Coffee (12oz ground)",     price: 8.99, aisle: "G3", category: "beverages", weightG: 340,  tags: ["breakfast", "drink", "coffee"] },
  { barcode: "012345678929", name: "Almond Milk (64oz)",       price: 3.99, aisle: "G1", category: "beverages", weightG: 1920, tags: ["drink", "dairy", "breakfast", "healthy"] },

  // ── Frozen ───────────────────────────────────────────────────────────────
  { barcode: "012345678930", name: "Frozen Pizza (12\")", price: 5.99, aisle: "H2", category: "frozen",  weightG: 737,  tags: ["dinner", "snack", "party"] },
  { barcode: "012345678931", name: "Ice Cream (1 qt)",    price: 4.99, aisle: "H3", category: "frozen",  weightG: 473,  tags: ["dessert", "snack"] },
];

// Lookup by barcode
function findByBarcode(barcode) {
  return products.find(p => p.barcode === barcode) || null;
}

// Search by name (for webapp)
function searchByName(query) {
  const q = query.toLowerCase();
  return products.filter(p => p.name.toLowerCase().includes(q));
}

// Get all products
function getAllProducts() {
  return products;
}

module.exports = { findByBarcode, searchByName, getAllProducts, products };
