import React from 'react';
import { View, Button } from 'react-native';
import { render, fireEvent } from '@testing-library/react-native';
import { renderHook } from '@testing-library/react-hooks';
import Animated, {
  useSharedValue,
  useAnimatedStyle,
  withTiming,
} from '../src/';
import {
  withReanimatedTimer,
  advanceAnimationByTime,
  advanceAnimationByFrame,
  getAnimatedStyle,
} from '../src/reanimated2/jestUtils';

const AnimatedSharedValueComponent = (props) => {
  const widthSV = props.sharedValue;

  const style = useAnimatedStyle(() => {
    return {
      width: withTiming(widthSV.value, { duration: 500 }),
    };
  });

  return (
    <View style={{ flex: 1, flexDirection: 'column' }}>
      <Animated.View
        testID="view"
        style={[
          { width: 0, height: 80, backgroundColor: 'black', margin: 30 },
          style,
        ]}
      />
      <Button
        testID="button"
        title="toggle"
        onPress={() => {
          widthSV.value = 100;
        }}
      />
    </View>
  );
};

const AnimatedComponent = () => {
  return <AnimatedSharedValueComponent sharedValue={useSharedValue(0)} />;
};

const getDefaultStyle = () => ({
  width: 0,
  height: 80,
  backgroundColor: 'black',
  margin: 30,
});

describe('Tests of animations', () => {
  test('withTiming animation', async () => {
    jest.useFakeTimers();
    const style = getDefaultStyle();

    const { getByTestId } = render(<AnimatedComponent />);
    const view = getByTestId('view');
    const button = getByTestId('button');

    expect(view.props.style.width).toBe(0);
    expect(view).toHaveAnimatedStyle(style);
    fireEvent.press(button);

    jest.runAllTimers();

    style.width = 100;
    expect(view).toHaveAnimatedStyle(style);
  });

  test('withTiming animation, get animated style', async () => {
    jest.useFakeTimers();
    const { getByTestId } = render(<AnimatedComponent />);
    const view = getByTestId('view');
    const button = getByTestId('button');
    fireEvent.press(button);
    jest.runAllTimers();
    const style = getAnimatedStyle(view);
    expect(style.width).toBe(100);
  });

  test('withTiming animation, width in a middle of animation', () => {
    withReanimatedTimer(() => {
      const style = getDefaultStyle();

      const { getByTestId } = render(<AnimatedComponent />);
      const view = getByTestId('view');
      const button = getByTestId('button');

      expect(view.props.style.width).toBe(0);
      expect(view).toHaveAnimatedStyle(style);

      fireEvent.press(button);
      advanceAnimationByTime(260);
      style.width = 46.08; // value of component width after 260ms of animation
      expect(view).toHaveAnimatedStyle(style);
    });
  });

  test('withTiming animation, use animation timer and advance by 10 frames of animation', () => {
    withReanimatedTimer(() => {
      const { getByTestId } = render(<AnimatedComponent />);
      const view = getByTestId('view');
      const button = getByTestId('button');

      fireEvent.press(button);
      advanceAnimationByFrame(10);
      // value of component width after 10 frames of animation
      expect(view).toHaveAnimatedStyle({ width: 16.588799999999996 });
    });
  });

  test('withTiming animation, compare all styles', () => {
    withReanimatedTimer(() => {
      const style = getDefaultStyle();

      const { getByTestId } = render(<AnimatedComponent />);
      const view = getByTestId('view');
      const button = getByTestId('button');

      fireEvent.press(button);
      advanceAnimationByTime(260);
      style.width = 46.08; // value of component width after 260ms of animation
      expect(view).toHaveAnimatedStyle(style, true);
    });
  });

  test('withTiming animation, define shared value outside component', () => {
    withReanimatedTimer(() => {
      let sharedValue;
      renderHook(() => {
        sharedValue = useSharedValue(0);
      });
      const { getByTestId } = render(
        <AnimatedComponent sharedValue={sharedValue} />
      );
      const view = getByTestId('view');
      const button = getByTestId('button');

      fireEvent.press(button);
      advanceAnimationByTime(260);
      // value of component width after 260ms of animation
      expect(view).toHaveAnimatedStyle({ width: 46.08 });
    });
  });

  test('withTiming animation, change shared value outside component', () => {
    jest.useFakeTimers();
    let sharedValue;
    renderHook(() => {
      sharedValue = useSharedValue(0);
    });
    const { getByTestId } = render(
      <AnimatedSharedValueComponent sharedValue={sharedValue} />
    );
    const view = getByTestId('view');
    sharedValue.value = 50;
    jest.runAllTimers();
    expect(view).toHaveAnimatedStyle({ width: 50 });
  });
});
