module.exports = {
  presets: ['module:metro-react-native-babel-preset'],
  plugins: [['../plugin', {processNestedWorklets: true}]],
};
