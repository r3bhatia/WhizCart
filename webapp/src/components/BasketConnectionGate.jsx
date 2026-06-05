export default function BasketConnectionGate({ cartId, onConfirm, onReject }) {
  if (!cartId) {
    return (
      <div className="connection-gate">
        <div className="card connection-panel">
          <p className="section-title">No basket connected</p>
          <h1>No Connection to the Basket</h1>
          <p>Please scan the basket QR code and confirm usage to access WhizCart.</p>
        </div>
      </div>
    );
  }

  return (
    <div className="connection-gate">
      <div className="card connection-panel">
        <p className="section-title">Basket pairing</p>
        <h1>Connect to {cartId}?</h1>
        <p>This will link your phone to this basket so you can search items, view the running cart, and check out.</p>
        <div className="connection-actions">
          <button className="btn btn-primary" onClick={onConfirm}>Yes, connect</button>
          <button className="btn btn-danger" onClick={onReject}>No</button>
        </div>
      </div>
    </div>
  );
}
