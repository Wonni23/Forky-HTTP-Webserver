#!/usr/bin/php-cgi
<?php
    // PHP CGI test
    header("Content-Type: text/html");
    header("Status: 200 OK");
?>
<html>
    <head>
        <title>PHP CGI Test</title>
    </head>
    <body>
        <h1>Hello from PHP CGI!</h1>

        <h2>PHP Version</h2>
        <p><?php echo phpversion(); ?></p>

        <h2>Request Information</h2>
        <ul>
            <li><b>REQUEST_METHOD:</b> <?php echo $_SERVER['REQUEST_METHOD'] ?? 'N/A'; ?></li>
            <li><b>QUERY_STRING:</b> <?php echo $_SERVER['QUERY_STRING'] ?? 'N/A'; ?></li>
            <li><b>CONTENT_LENGTH:</b> <?php echo $_SERVER['CONTENT_LENGTH'] ?? 'N/A'; ?></li>
            <li><b>CONTENT_TYPE:</b> <?php echo $_SERVER['CONTENT_TYPE'] ?? 'N/A'; ?></li>
        </ul>

        <h2>GET Parameters</h2>
        <?php if (!empty($_GET)): ?>
            <ul>
                <?php foreach ($_GET as $key => $value): ?>
                    <li><b><?php echo htmlspecialchars($key); ?>:</b> <?php echo htmlspecialchars($value); ?></li>
                <?php endforeach; ?>
            </ul>
        <?php else: ?>
            <p>No GET parameters</p>
        <?php endif; ?>

        <h2>POST Parameters</h2>
        <?php if (!empty($_POST)): ?>
            <ul>
                <?php foreach ($_POST as $key => $value): ?>
                    <li><b><?php echo htmlspecialchars($key); ?>:</b> <?php echo htmlspecialchars($value); ?></li>
                <?php endforeach; ?>
            </ul>
        <?php else: ?>
            <p>No POST parameters</p>
        <?php endif; ?>
    </body>
</html>
