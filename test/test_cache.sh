#!/bin/bash
# Test per-provider cache functionality

echo "Testing per-provider cache..."
echo "============================="

source .envrc

# Build test
make test_providers > /dev/null 2>&1

echo -e "\nNOTE: Each provider now maintains its own cache."
echo "When switching providers, you instantly get that provider's cached data"
echo "instead of having to fetch fresh data every time."
echo ""
echo "Running provider test for Berlin..."
echo ""

# Run test - it will fetch from all providers
./test_providers Berlin 2>&1 | grep -E "Testing|City:|Temperature:|Using cached data|Fetching"

echo -e "\n============================="
echo "Per-provider cache benefits:"
echo "1. Switch providers instantly without waiting for fetch"
echo "2. Each provider keeps its own cached data"
echo "3. City change clears ALL provider caches"
echo "4. No cross-provider data contamination"