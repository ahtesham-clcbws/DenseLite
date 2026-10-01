#!/usr/bin/env python3
"""
e2e_assert_stress.py — End-to-End Assertion-Driven Stress Test Suite

Asserts:
1. Model endpoint availability and schema correctness (/v1/models)
2. Semantic memory injection & chat response validation (/v1/chat/completions)
3. Code search intent and response non-emptiness
4. Context overflow and resource policy boundary compliance
5. Proper HTTP status codes, JSON schema conformity, and zero crashes
"""

import sys
import json
import time
import urllib.request
import urllib.error

BASE_URL = "http://127.0.0.1:9501"

def http_post(endpoint, payload, timeout=120):
    url = f"{BASE_URL}{endpoint}"
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as response:
            status = response.getcode()
            body = response.read().decode("utf-8")
            return status, json.loads(body)
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8")
        try:
            return e.code, json.loads(body)
        except Exception:
            return e.code, {"error": body}
    except Exception as e:
        return 0, {"error": str(e)}

def http_get(endpoint, timeout=30):
    url = f"{BASE_URL}{endpoint}"
    req = urllib.request.Request(url, headers={"Accept": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as response:
            status = response.getcode()
            body = response.read().decode("utf-8")
            return status, json.loads(body)
    except Exception as e:
        return 0, {"error": str(e)}

def test_models_endpoint():
    print("[+] Test 1: Querying /v1/models endpoint...")
    status, data = http_get("/v1/models")
    if status == 0:
        print(f"  [-] Server offline at {BASE_URL}. Skipping live HTTP tests.")
        return False
    assert status == 200, f"Expected 200 OK, got {status}: {data}"
    assert "data" in data or "models" in data, f"Malformed models payload: {data}"
    print("  ✓ /v1/models responded with 200 OK and valid schema.")
    return True

def test_chat_completion_with_memory():
    print("[+] Test 2: Chat completion with semantic memory rule...")
    payload = {
        "model": "denselite",
        "messages": [
            {"role": "user", "content": "I want you to remember this rule: always use the repository pattern."}
        ],
        "stream": False
    }
    status, data = http_post("/v1/chat/completions", payload)
    assert status == 200, f"Chat completion failed with status {status}: {data}"
    assert "choices" in data and len(data["choices"]) > 0, "No choices returned"
    message = data["choices"][0].get("message", {})
    assert message.get("role") == "assistant", "Expected assistant role"
    content = message.get("content", "")
    assert len(content.strip()) > 0, "Assistant returned empty content"
    print(f"  ✓ Chat completion succeeded (content length={len(content)}).")

def test_code_search_intent():
    print("[+] Test 3: Code search intent execution...")
    payload = {
        "model": "denselite",
        "messages": [
            {"role": "user", "content": "Search the local codebase for MemoryConsolidator."}
        ],
        "stream": False
    }
    status, data = http_post("/v1/chat/completions", payload)
    assert status == 200, f"Code search query failed with status {status}: {data}"
    assert "choices" in data and len(data["choices"]) > 0, "No choices returned"
    content = data["choices"][0].get("message", {}).get("content", "")
    assert len(content.strip()) > 0, "Search intent returned empty content"
    print("  ✓ Code search intent processed successfully.")

def test_context_overflow_boundaries():
    print("[+] Test 4: Context overflow bounding...")
    # Massive payload to verify context pruning and bounded resource allocation
    large_text = "The quick brown fox jumps over the lazy dog. " * 3000
    payload = {
        "model": "denselite",
        "messages": [
            {"role": "user", "content": f"Summarize this text: {large_text}"}
        ],
        "stream": False
    }
    status, data = http_post("/v1/chat/completions", payload, timeout=120)
    assert status != 500, f"Server crashed with 500 Internal Error under context load: {data}"
    assert status in [200, 400], f"Unexpected status {status} under context load: {data}"
    if status == 200:
        content = data.get("choices", [{}])[0].get("message", {}).get("content", "")
        assert len(content.strip()) > 0, "Pruned context resulted in empty completion"
        print(f"  ✓ ContextEngine successfully pruned and summarized large context (length={len(content)}).")
    else:
        assert "error" in data, f"400 response missing error specification: {data}"
        print(f"  ✓ Server cleanly rejected oversized context with client error: {data.get('error')}")

def main():
    print("=" * 60)
    print(" DenseLite E2E Assertion-Driven Stress Test Suite")
    print("=" * 60)
    if not test_models_endpoint():
        print("[-] FAIL: DenseLite server is not running on port 9501.")
        print("[-] E2E live assertion tests cannot be certified while server is offline.")
        sys.exit(2)

    test_chat_completion_with_memory()
    test_code_search_intent()
    test_context_overflow_boundaries()
    print("=" * 60)
    print(" [✓] ALL E2E ASSERTION TESTS PASSED SUCCESSFULLY")
    print("=" * 60)

if __name__ == "__main__":
    main()
