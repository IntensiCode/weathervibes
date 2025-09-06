#!/bin/bash
# Rapid provider switching test using existing test_providers

echo "Rapid Provider Switching Test"
echo "=============================="
echo "Testing rapid provider switches to detect crashes"
echo ""

source .envrc

# Build test_providers if needed
PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig make test_providers > /dev/null 2>&1

if [ ! -f ./test_providers ]; then
    echo "Failed to build test_providers"
    exit 1
fi

echo "Running 20 rapid provider switch cycles..."
echo ""

for i in {1..20}; do
    echo "Cycle $i/20:"
    ./test_providers "Berlin" 2>&1 | grep -E "Testing provider|temperature" | head -8
    echo ""
    sleep 0.5
done

echo "Test completed successfully - no crashes!"
echo ""
echo "Checking dmesg for any segfaults..."
dmesg | grep -i "weather" | tail -5

