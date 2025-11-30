#!/bin/bash
# Automatischer Test für TW-Mailer Client

echo "Testing TW-Mailer..."

# Test 1: SEND Message
echo "Test 1: Sending message from alice to bob..."
(
echo "1"           # SEND auswählen
echo "alice"       # Sender
echo "bob"         # Receiver
echo "Test Mail"   # Subject
echo "Hello Bob!"  # Message Zeile 1
echo "This is a test." # Message Zeile 2
echo "."           # Ende-Marker
sleep 1
echo "5"           # QUIT
) | ./twmailer-client 127.0.0.1 6543

echo ""
echo "Test 1 completed."
sleep 1

# Test 2: LIST Messages
echo ""
echo "Test 2: Listing messages for bob..."
(
echo "2"           # LIST auswählen
echo "bob"         # Username
sleep 1
echo "5"           # QUIT
) | ./twmailer-client 127.0.0.1 6543

echo ""
echo "Test 2 completed."
sleep 1

# Test 3: READ Message
echo ""
echo "Test 3: Reading message #1 for bob..."
(
echo "3"           # READ auswählen
echo "bob"         # Username
echo "1"           # Message-Nummer
sleep 1
echo "5"           # QUIT
) | ./twmailer-client 127.0.0.1 6543

echo ""
echo "Test 3 completed."
sleep 1

# Test 4: Send another message
echo ""
echo "Test 4: Sending second message..."
(
echo "1"           # SEND auswählen
echo "alice"       # Sender
echo "bob"         # Receiver
echo "Second Test" # Subject
echo "Another message."
echo "."           # Ende-Marker
sleep 1
echo "5"           # QUIT
) | ./twmailer-client 127.0.0.1 6543

echo ""
echo "Test 4 completed."
sleep 1

# Test 5: LIST again (should show 2 messages)
echo ""
echo "Test 5: Listing messages again (should show 2)..."
(
echo "2"           # LIST auswählen
echo "bob"         # Username
sleep 1
echo "5"           # QUIT
) | ./twmailer-client 127.0.0.1 6543

echo ""
echo "Test 5 completed."
sleep 1

# Test 6: DELETE Message
echo ""
echo "Test 6: Deleting message #1..."
(
echo "4"           # DEL auswählen
echo "bob"         # Username
echo "1"           # Message-Nummer
sleep 1
echo "5"           # QUIT
) | ./twmailer-client 127.0.0.1 6543

echo ""
echo "Test 6 completed."
sleep 1

# Test 7: LIST final (should show 1 message)
echo ""
echo "Test 7: Final list (should show 1 message)..."
(
echo "2"           # LIST auswählen
echo "bob"         # Username
sleep 1
echo "5"           # QUIT
) | ./twmailer-client 127.0.0.1 6543

echo ""
echo "Test 7 completed."
echo ""
echo "All tests completed!"
echo ""
echo "Check mailspool directory:"
ls -la mailspool/bob/
