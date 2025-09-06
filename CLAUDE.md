# CLAUDE.md - Project-specific instructions

## CRITICAL SECURITY RULE
**DO NOT USE HARDCODED API KEYS! ONLY RELY ON .envrc TO PROVIDE THEM.**
- Never put API keys directly in source code
- Always use `getenv("OPENWEATHER_API_KEY")` and `getenv("TOMORROW_API_KEY")`
- The .envrc file contains the keys for local development

## Build and Test
After making any code changes, ALWAYS run:
```bash
source .envrc
PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig make clean && make check
```

This ensures the code compiles and all tests pass before considering the work complete.

## Additional Guidelines (optional)
Consider adding these if needed:
- Mutex protection rules for shared data access
- Provider cache architecture notes
- Test coverage requirements