export module constants;

export namespace Constants {

// DPI and unit conversions
constexpr double kDisplayDpi = 96.0;           // Screen display DPI (standard Windows DPI)
constexpr double kMmPerInch = 25.4;            // Millimeters per inch
constexpr double kMmToPx = kDisplayDpi / kMmPerInch;  // Pixels per millimeter for display
constexpr double kPxToMm = kMmPerInch / kDisplayDpi;  // Millimeters per pixel for display

// Badge layout constants
constexpr double kCircleBleedMm = 3.0;        // Extra bleed for circular badges

// Image processing
constexpr int kMaxSvgSide = 2048;             // Maximum dimension for SVG rendering
constexpr int kDefaultSvgSize = 1024;         // Default size for SVG without viewBox

} // namespace Constants
