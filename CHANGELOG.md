# Changelog

All notable changes to Weather Vibes will be documented in this file.

## [1.0.0] - 2025-01-06

### Added
- Initial release of Weather Vibes MATE Panel Applet
- Multiple weather provider support:
  - AnsiWeather (global, no API key required)
  - Bright Sky (DWD - Germany)
  - OpenWeather (requires API key)
  - Tomorrow.io (requires API key)
- Emoji-based weather condition display
- Day/night detection using sunrise/sunset data
- GSettings configuration storage with automatic migration
- Comprehensive test suite with GLib test framework
- GitHub Actions CI/CD pipeline
- JSON parsing helper library
- Real-time weather updates with configurable intervals
- Temperature unit switching (Celsius/Fahrenheit)
- Detailed weather popup with toggle functionality

### Technical Improvements
- Modular architecture with clear separation of concerns
- Thread-safe asynchronous weather fetching
- Proper error handling and user feedback
- Secure HTTPS-only network operations
- Memory leak prevention with proper cleanup