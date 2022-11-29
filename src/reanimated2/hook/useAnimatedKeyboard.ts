import { useEffect, useRef } from 'react';
import {
  makeMutable,
  subscribeForKeyboardEvents,
  unsubscribeFromKeyboardEvents,
} from '../core';
import { AnimatedKeyboardInfo, KeyboardState } from '../commonTypes';

export function useAnimatedKeyboard(): AnimatedKeyboardInfo {
  const ref = useRef<AnimatedKeyboardInfo | null>(null);
  const listenerId = useRef<number>(-1);
  const isSubscribed = useRef<boolean>(false);

  if (ref.current === null) {
    const keyboardEventData: AnimatedKeyboardInfo = {
      state: makeMutable<KeyboardState>(KeyboardState.UNKNOWN),
      height: makeMutable(0),
    };
    listenerId.current = subscribeForKeyboardEvents((state, height) => {
      'worklet';
      keyboardEventData.state.value = state;
      keyboardEventData.height.value = height;
    });
    ref.current = keyboardEventData;
    isSubscribed.current = true;
  }
  useEffect(() => {
    if (isSubscribed.current === false && ref.current !== null) {
      const keyboardEventData = ref.current;
      // subscribe again after Fast Refresh
      listenerId.current = subscribeForKeyboardEvents((state, height) => {
        'worklet';
        keyboardEventData.state.value = state;
        keyboardEventData.height.value = height;
      });
      isSubscribed.current = true;
    }
    return () => {
      unsubscribeFromKeyboardEvents(listenerId.current);
      isSubscribed.current = false;
    };
  }, []);
  return ref.current;
}
