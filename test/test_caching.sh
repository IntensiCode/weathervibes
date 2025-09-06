#!/bin/bash

echo "Testing weather provider caching (1-minute cooldown)"
echo "====================================================="
echo

echo "First fetch for each provider (should all fetch):"
./test_providers Berlin 2>&1 | grep -E "Fetching|Skipping|Temperature:"

echo
echo "Waiting 5 seconds..."
sleep 5

echo
echo "Second fetch (within 1 minute - should skip all):"
./test_providers Berlin 2>&1 | grep -E "Fetching|Skipping|Temperature:"

echo
echo "Testing provider switch (each provider has its own cache):"
echo "Fetching with AnsiWeather only:"
./test_providers Berlin 2>&1 | grep -A1 "Testing AnsiWeather" | grep -E "Fetching|Skipping"

echo
echo "Note: To see full caching in action, wait 60 seconds and run again."