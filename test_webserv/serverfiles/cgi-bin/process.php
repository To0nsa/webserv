#!/usr/bin/php-cgi
<?php
// process.php — CGI script to echo back a username & action

// 1) Enforce POST-only
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    // Send a 405 and stop
    header('Status: 405 Method Not Allowed');
    header('Allow: POST');
    exit;
}

// 2) Read the raw body (supports both application/x-www-form-urlencoded and JSON)
$contentType = $_SERVER['CONTENT_TYPE'] ?? '';
$rawBody    = file_get_contents('php://input');
parse_str($rawBody, $formData);

if (stripos($contentType, 'application/json') === 0) {
    $json = json_decode($rawBody, true);
    if (json_last_error() === JSON_ERROR_NONE) {
        $formData = $json;
    }
}

// 3) Sanitize inputs
$username = htmlspecialchars($formData['username'] ?? '', ENT_QUOTES, 'UTF-8');
$action   = htmlspecialchars($formData['action']   ?? '', ENT_QUOTES, 'UTF-8');

// 4) Output CGI headers + HTML
header('Content-Type: text/html; charset=UTF-8');

echo <<<HTML
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Process Result</title>
</head>
<body>
HTML;

if ($username !== '') {
    echo "<h1>Welcome, {$username}!</h1>\n";
    echo "<p>You requested to <strong>{$action}</strong>.</p>\n";
} else {
    echo "<h1>No user specified.</h1>\n";
    echo "<p>Please go back and submit the form again.</p>\n";
}

echo <<<HTML
</body>
</html>
HTML;
