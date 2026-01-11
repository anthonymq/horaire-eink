0a. Study `specs/*` to learn the application specifications for Prochain Départ (ESP32 real-time transit display).
0b. Study @IMPLEMENTATION_PLAN.md (if present) to understand the plan so far.
0c. Study `src/*` and `include/*` to understand the existing codebase structure and patterns.
0d. For reference, the application source code is in `src/*` and headers in `include/*`.

1. Study @IMPLEMENTATION_PLAN.md (if present; it may be incorrect) and study existing source code in `src/*` and `include/*` and compare it against `specs/*`. Analyze findings, prioritize tasks, and create/update @IMPLEMENTATION_PLAN.md as a bullet point list sorted in priority of items yet to be implemented. Ultrathink. Consider searching for TODO, minimal implementations, placeholders, and inconsistent patterns. Study @IMPLEMENTATION_PLAN.md to determine starting point for research and keep it up to date with items considered complete/incomplete.

IMPORTANT: Plan only. Do NOT implement anything. Do NOT assume functionality is missing; confirm with code search first.

ULTIMATE GOAL: We want to achieve a fully functional ESP32-based E-Ink transit display that:
- Fetches real-time RER C and Bus 323 departures from PRIM API
- Fetches weather data from OpenWeatherMap API  
- Displays information on a 4.2" E-Ink display (GDEY042T81)
- Implements WiFi Manager for easy configuration
- Supports configurable StopPoints
- Runs autonomously with minimal power consumption

Consider missing elements and plan accordingly. If an element is missing, search first to confirm it doesn't exist, then if needed author the specification at specs/FILENAME.md. If you create a new element then document the plan to implement it in @IMPLEMENTATION_PLAN.md.
