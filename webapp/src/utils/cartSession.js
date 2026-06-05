import { useEffect, useMemo, useState } from "react";
import { useSearchParams } from "react-router-dom";

const DEFAULT_CART_ID = "";
const STORAGE_KEY = "whizcart.cartId";
const CONFIRMED_KEY_PREFIX = "whizcart.confirmed.";

export function useCartSession() {
  const [searchParams, setSearchParams] = useSearchParams();
  const urlCartId = searchParams.get("cartId");
  const [confirmedToken, setConfirmedToken] = useState(0);

  const cartId = useMemo(() => {
    return urlCartId || localStorage.getItem(STORAGE_KEY) || DEFAULT_CART_ID;
  }, [urlCartId]);

  const isConfirmed = useMemo(() => {
    return cartId
      ? localStorage.getItem(`${CONFIRMED_KEY_PREFIX}${cartId}`) === "yes"
      : false;
  }, [cartId, confirmedToken]);

  const needsConfirmation = Boolean(cartId && !isConfirmed);

  useEffect(() => {
    if (cartId && isConfirmed) {
      localStorage.setItem(STORAGE_KEY, cartId);
    }
    if (!urlCartId && cartId && isConfirmed) {
      const nextParams = new URLSearchParams(searchParams);
      nextParams.set("cartId", cartId);
      setSearchParams(nextParams, { replace: true });
    }
  }, [cartId, isConfirmed, searchParams, setSearchParams, urlCartId]);

  async function confirmCart() {
    if (!cartId) return;
    try {
      await fetch("/api/cart/connect", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ cartId, connected: true }),
      });
    } catch (error) {
      console.warn("Could not notify basket display about connection", error);
    }
    localStorage.setItem(`${CONFIRMED_KEY_PREFIX}${cartId}`, "yes");
    localStorage.setItem(STORAGE_KEY, cartId);
    setConfirmedToken(value => value + 1);
  }

  async function rejectCart() {
    if (cartId) {
      try {
        await fetch("/api/cart/connect", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ cartId, connected: false }),
        });
      } catch (error) {
        console.warn("Could not notify basket display about rejection", error);
      }
      localStorage.removeItem(`${CONFIRMED_KEY_PREFIX}${cartId}`);
    }
    localStorage.removeItem(STORAGE_KEY);
    setConfirmedToken(value => value + 1);
  }

  function pathWithCart(path) {
    if (!cartId) return path;
    return `${path}?cartId=${encodeURIComponent(cartId)}`;
  }

  return { cartId, isConfirmed, needsConfirmation, confirmCart, rejectCart, pathWithCart };
}
