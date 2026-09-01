# Compatibility and On-Device Limits

Desktop Studio and WebSDK Browser Studio aim to match the Bridge methods,
events, and GRAY_4 bitmap contract.
The initial Canvas text implementation is not a pixel-identical replacement
for firmware LVGL rendering and proprietary fonts. Brightness, optical
distance, lens distortion, and actual gesture thresholds must still be
validated on physical glasses.

Every simulator run should display its Bridge version and device profile.
Future profiles will be generated from versioned firmware definitions to
prevent manually copied constants from drifting. Only Desktop Studio executes
glasses `.gmp` files; Browser Studio covers Web-only Bridge and rendering tests.
