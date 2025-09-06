#include <stdio.h>
#include <string.h>
#include "../src/network.h"
#include "../src/logger.h"

int test_https_fetch() {
    printf("Testing HTTPS fetch with libsoup...\n");
    
    // Test a simple HTTPS endpoint
    char *response = network_fetch_url("https://api.github.com/zen");
    if (response) {
        printf("✅ HTTPS fetch succeeded: %zu bytes\n", strlen(response));
        printf("   Content: %.50s%s\n", response, strlen(response) > 50 ? "..." : "");
        g_free(response);
        return 1;
    } else {
        printf("❌ HTTPS fetch failed\n");
        return 0;
    }
}

int test_http_to_https_upgrade() {
    printf("\nTesting HTTP to HTTPS upgrade...\n");
    
    // This should be auto-upgraded to HTTPS
    char *response = network_fetch_url("http://api.github.com/zen");
    if (response) {
        printf("✅ HTTP→HTTPS upgrade succeeded\n");
        g_free(response);
        return 1;
    } else {
        printf("❌ HTTP→HTTPS upgrade failed\n");
        return 0;
    }
}

int test_json_fetch() {
    printf("\nTesting JSON fetch...\n");
    
    char *response = network_fetch_json("https://api.github.com/repos/torvalds/linux");
    if (response) {
        // Check if it's valid JSON (has expected fields)
        if (strstr(response, "\"name\"") && strstr(response, "\"linux\"")) {
            printf("✅ JSON fetch succeeded and contains expected fields\n");
            g_free(response);
            return 1;
        } else {
            printf("❌ JSON fetch succeeded but unexpected content\n");
            g_free(response);
            return 0;
        }
    } else {
        printf("❌ JSON fetch failed\n");
        return 0;
    }
}

int test_404_handling() {
    printf("\nTesting 404 error handling...\n");
    
    char *response = network_fetch_url("https://api.github.com/this-does-not-exist-404");
    if (response) {
        printf("❌ Should have failed for 404 but got response\n");
        g_free(response);
        return 0;
    } else {
        printf("✅ Correctly returned NULL for 404\n");
        return 1;
    }
}

int test_invalid_url() {
    printf("\nTesting invalid URL handling...\n");
    
    char *response = network_fetch_url("not-a-valid-url");
    if (response) {
        printf("❌ Should have failed for invalid URL\n");
        g_free(response);
        return 0;
    } else {
        printf("✅ Correctly returned NULL for invalid URL\n");
        return 1;
    }
}

int main() {
    logger_init();
    
    printf("LibSoup Network Test Suite\n");
    printf("==========================\n\n");
    
    int passed = 0;
    int total = 5;
    
    passed += test_https_fetch();
    passed += test_http_to_https_upgrade();
    passed += test_json_fetch();
    passed += test_404_handling();
    passed += test_invalid_url();
    
    printf("\n==========================\n");
    printf("Results: %d/%d tests passed\n", passed, total);
    
    if (passed == total) {
        printf("✅ All tests PASSED!\n");
    } else {
        printf("❌ Some tests FAILED\n");
    }
    
    network_cleanup();
    logger_cleanup();
    
    return (passed == total) ? 0 : 1;
}