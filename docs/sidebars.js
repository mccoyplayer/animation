module.exports = {
  docs: {
    Fundamentals: [
      'about',
      'installation',
      'worklets',
      'shared-values',
      'animations',
      'events',
      'architecture',
      'migration',
      'troubleshooting',
    ],
    'API Reference': [
      {
        Hooks: [
          'api/useSharedValue',
          'api/useAnimatedStyle',
          'api/useDerivedValue',
          'api/useAnimatedScrollHandler',
          'api/useAnimatedGestureHandler',
          'api/useAnimatedRef',
          'api/useAnimatedReaction',
        ],
        Animations: [
          'api/withTiming',
          'api/withSpring',
          'api/withDecay',
          'api/cancelAnimation',
          'api/withDelay',
          'api/withSequence',
          'api/withRepeat',
        ],
        'Native methods': [
          'api/nativeMethods/measure',
          'api/nativeMethods/scrollTo',
        ],
      },
    ],
  },
};
