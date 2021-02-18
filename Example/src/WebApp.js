import React from 'react';
import { FlatList, StyleSheet, Text, View, LogBox } from 'react-native';

import { RectButton, ScrollView } from 'react-native-gesture-handler';

import { createStackNavigator } from '@react-navigation/stack';
import { NavigationContainer } from '@react-navigation/native';

import AnimatedStyleUpdateExample from './AnimatedStyleUpdateExample';
import WobbleExample from './WobbleExample';
import DragAndSnapExample from './DragAndSnapExample';
import ScrollEventExample from './ScrollEventExample';
import ChatHeadsExample from './ChatHeadsExample';
import MeasureExample from './MeasureExample';
import SwipeableListExample from './SwipeableListExample';
import ScrollableViewExample from './ScrollableViewExample';
import ScrollToExample from './ScrollToExample';
/* font awesome does not work * /
import AnimatedTabBarExample from './AnimatedTabBarExample';
/**/
import LightboxExample from './WebSpecific/LightBoxExample';
/* masked view does not work * /
import LiquidSwipe from './LiquidSwipe';
/**/
LogBox.ignoreLogs(['Calling `getNode()`']);

const SCREENS = {
  AnimatedStyleUpdate: {
    screen: AnimatedStyleUpdateExample,
    title: '🆕 Animated Style Update',
  },
  WobbleExample: {
    screen: WobbleExample,
    title: '🆕 Animation Modifiers (Wobble Effect)',
  },
  DragAndSnapExample: {
    screen: DragAndSnapExample,
    title: '🆕 Drag and Snap',
  },
  MeasureExample: {
    screen: MeasureExample,
    title: '🆕 Synchronous Measure',
  },
  ScrollEventExample: {
    screen: ScrollEventExample,
    title: '🆕 Scroll Events',
  },
  ChatHeadsExample: {
    screen: ChatHeadsExample,
    title: '🆕 Chat Heads',
  },
  ScrollableToExample: {
    screen: ScrollToExample,
    title: '🆕 scrollTo',
  },
  SwipeableListExample: {
    screen: SwipeableListExample,
    title: '🆕 (advanced) Swipeable List',
  },
  LightboxExample: {
    screen: LightboxExample,
    title: '🆕 (advanced) Lightbox',
  },
  ScrollableViewExample: {
    screen: ScrollableViewExample,
    title: '🆕 (advanced) ScrollView imitation',
  },
  /** /
  AnimatedTabBarExample: {
    screen: AnimatedTabBarExample,
    title: '🆕 (advanced) Tab Bar Example',
  },
  /** /
  LiquidSwipe: {
    screen: LiquidSwipe,
    title: '🆕 Liquid Swipe Example',
  },
  /**/
};

function MainScreen({ navigation }) {
  const data = Object.keys(SCREENS).map((key) => ({ key }));
  return (
    <FlatList
      style={styles.list}
      data={data}
      ItemSeparatorComponent={ItemSeparator}
      renderItem={(props) => (
        <MainScreenItem
          {...props}
          screens={SCREENS}
          onPressItem={({ key }) => navigation.navigate(key)}
        />
      )}
      renderScrollComponent={(props) => <ScrollView {...props} />}
    />
  );
}

export function ItemSeparator() {
  return <View style={styles.separator} />;
}

export function MainScreenItem({ item, onPressItem, screens }) {
  const { key } = item;
  return (
    <RectButton style={styles.button} onPress={() => onPressItem(item)}>
      <Text style={styles.buttonText}>{screens[key].title || key}</Text>
    </RectButton>
  );
}

const Stack = createStackNavigator();

export default function App() {
  return (
    <NavigationContainer>
      <Stack.Navigator>
        <Stack.Screen
          name="Home"
          options={{ title: '🎬 Reanimated 2.x Examples' }}
          component={MainScreen}
        />
        {Object.keys(SCREENS).map((name) => (
          <Stack.Screen
            key={name}
            name={name}
            getComponent={() => SCREENS[name].screen}
            options={{ title: SCREENS[name].title || name }}
          />
        ))}
      </Stack.Navigator>
    </NavigationContainer>
  );
}

export const styles = StyleSheet.create({
  list: {
    backgroundColor: '#EFEFF4',
  },
  separator: {
    height: 1,
    backgroundColor: '#DBDBE0',
  },
  buttonText: {
    backgroundColor: 'transparent',
  },
  button: {
    flex: 1,
    height: 60,
    padding: 10,
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#fff',
  },
});
