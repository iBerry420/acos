#!/usr/bin/env python3
"""Extract the monolithic EmbeddedAssets.hpp into separate UI files."""

import os
import re
import sys

EMBEDDED = os.path.join(os.path.dirname(__file__), '..', 'src', 'server', 'EmbeddedAssets.hpp')
UI_DIR = os.path.join(os.path.dirname(__file__), '..', 'ui')

def extract():
    with open(EMBEDDED, 'r') as f:
        content = f.read()

    # Extract the HTML from getIndexHtml() raw string literal
    m = re.search(r'return R"html\((.*?)\)html"', content, re.DOTALL)
    if not m:
        print("ERROR: Could not find getIndexHtml() raw string literal")
        sys.exit(1)

    html = m.group(1)

    # Extract CSS (between first <style> and </style>)
    css_match = re.search(r'<style>\s*(.*?)\s*</style>', html, re.DOTALL)
    if not css_match:
        print("ERROR: Could not find <style> block")
        sys.exit(1)
    full_css = css_match.group(1)

    # Split CSS variables from the rest
    # The :root block is at the start
    root_match = re.match(r'(:root\{[^}]+\})\s*(.*)', full_css, re.DOTALL)
    if root_match:
        variables_css = root_match.group(1)
        rest_css = root_match.group(2)
    else:
        variables_css = ''
        rest_css = full_css

    # Extract JS (between <script> immediately after <body> content and </script></body>)
    # There may be multiple <script> tags; the CDN one is before </head>, the main one is in <body>
    # Find the main app script (the big IIFE)
    js_match = re.search(r'<script>\s*\(function\(\)\{(.*?)\}\)\(\);\s*</script>\s*</body>', html, re.DOTALL)
    if not js_match:
        print("ERROR: Could not find main app <script> block")
        sys.exit(1)
    app_js = '(function(){\n"use strict";\n' + js_match.group(1).strip() + '\n})();'

    # Extract the HTML body content (between <body> and the main <script>)
    body_match = re.search(r'<body>\s*(.*?)\s*<script>\s*\(function', html, re.DOTALL)
    if not body_match:
        print("ERROR: Could not find body HTML")
        sys.exit(1)
    body_html = body_match.group(1).strip()

    # Extract any CDN scripts from <head>
    cdn_scripts = re.findall(r'(<script\s+src="[^"]+"></script>)', html)

    # Create directory structure
    os.makedirs(os.path.join(UI_DIR, 'css'), exist_ok=True)
    os.makedirs(os.path.join(UI_DIR, 'js'), exist_ok=True)
    os.makedirs(os.path.join(UI_DIR, 'themes'), exist_ok=True)

    # Write variables.css
    with open(os.path.join(UI_DIR, 'css', 'variables.css'), 'w') as f:
        f.write('/* Avacli Theme Variables — edit these to customize the look */\n')
        f.write(variables_css + '\n')
    print(f"  css/variables.css ({len(variables_css)} bytes)")

    # Write style.css
    with open(os.path.join(UI_DIR, 'css', 'style.css'), 'w') as f:
        f.write('/* Avacli UI Styles */\n')
        f.write(rest_css + '\n')
    print(f"  css/style.css ({len(rest_css)} bytes)")

    # Write app.js
    with open(os.path.join(UI_DIR, 'js', 'app.js'), 'w') as f:
        f.write(app_js + '\n')
    print(f"  js/app.js ({len(app_js)} bytes)")

    # Write default theme (empty — uses variables.css as-is)
    with open(os.path.join(UI_DIR, 'themes', 'default.css'), 'w') as f:
        f.write('/* Default theme — no overrides, uses variables.css as-is */\n')
    print(f"  themes/default.css")

    # Build index.html
    cdn_tags = '\n'.join(cdn_scripts)
    index_html = f'''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,interactive-widget=resizes-content,viewport-fit=cover">
<title>Avacli</title>
<link rel="stylesheet" href="/ui/css/variables.css">
<link rel="stylesheet" href="/ui/css/style.css">
<link id="theme-css" rel="stylesheet" href="/ui/themes/default.css">
{cdn_tags}
</head>
<body>
{body_html}
<script src="/ui/js/app.js"></script>
</body>
</html>'''

    with open(os.path.join(UI_DIR, 'index.html'), 'w') as f:
        f.write(index_html + '\n')
    print(f"  index.html ({len(index_html)} bytes)")

    print(f"\nExtracted UI to {os.path.abspath(UI_DIR)}/")


if __name__ == '__main__':
    print("Extracting UI from EmbeddedAssets.hpp...")
    extract()
    print("Done.")
