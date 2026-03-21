#!/bin/bash
#
# Setup Git Hooks for AdaptivePWM
# CISSP-Aligned Development Workflow
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "====================================="
echo "AdaptivePWM Git Hooks Setup"
echo "CISSP-Aligned Security Framework"
echo "====================================="
echo ""

# Check if we're in a git repository
if [[ ! -d "$PROJECT_ROOT/.git" ]]; then
    echo "Error: Not a git repository"
    echo "Please run: git init"
    exit 1
fi

# Create hooks directory if needed
mkdir -p "$PROJECT_ROOT/.git/hooks"

# Copy pre-commit hook
echo "Installing pre-commit hook..."
if [[ -f "$PROJECT_ROOT/git-hooks/pre-commit" ]]; then
    cp "$PROJECT_ROOT/git-hooks/pre-commit" "$PROJECT_ROOT/.git/hooks/pre-commit"
    chmod +x "$PROJECT_ROOT/.git/hooks/pre-commit"
    echo "✓ pre-commit hook installed"
else
    echo "✗ pre-commit hook not found in git-hooks/"
    exit 1
fi

# Copy commit-msg hook (optional, for message validation)
echo "Installing commit-msg hook..."
cat > "$PROJECT_ROOT/.git/hooks/commit-msg" << 'EOF'
#!/bin/bash
# Commit message validation for AdaptivePWM

COMMIT_MSG_FILE="$1"
COMMIT_MSG=$(cat "$COMMIT_MSG_FILE")

# Check conventional commit format
if ! echo "$COMMIT_MSG" | grep -qE "^(feat|fix|docs|style|refactor|test|chore|security|ci|merge)(\(.+\))?: .+"; then
    echo "Warning: Commit message doesn't follow conventional format"
    echo "Expected: type(scope): subject"
    echo "Types: feat, fix, docs, style, refactor, test, chore, security, ci"
    echo ""
    echo "Example: feat(pwm): add 16MHz HSE clock support"
    echo ""
    # Non-blocking for now
fi

# Check line length (subject should be <= 72)
SUBJECT_LINE=$(echo "$COMMIT_MSG" | head -1)
if [[ ${#SUBJECT_LINE} -gt 72 ]]; then
    echo "Warning: Subject line exceeds 72 characters (${#SUBJECT_LINE})"
fi

exit 0
EOF
chmod +x "$PROJECT_ROOT/.git/hooks/commit-msg"
echo "✓ commit-msg hook installed"

# Create post-checkout hook for documentation reminder
echo "Installing post-checkout hook..."
cat > "$PROJECT_ROOT/.git/hooks/post-checkout" << 'EOF'
#!/bin/bash
# Post-checkout hook for AdaptivePWM

PREVIOUS_HEAD="$1"
NEW_HEAD="$2"
BRANCH_SWITCH="$3"

if [[ "$BRANCH_SWITCH" == "1" ]]; then
    BRANCH=$(git rev-parse --abbrev-ref HEAD)
    echo ""
    echo "========================================"
    echo "Switched to branch: $BRANCH"
    echo "========================================"
    echo ""
    echo "Remember: Framework compliance required for commits"
    echo "Run: ./ci/enforce_framework.sh"
    echo ""
fi
EOF
chmod +x "$PROJECT_ROOT/.git/hooks/post-checkout"
echo "✓ post-checkout hook installed"

# Create post-merge hook
echo "Installing post-merge hook..."
cat > "$PROJECT_ROOT/.git/hooks/post-merge" << 'EOF'
#!/bin/bash
# Post-merge hook for AdaptivePWM

echo ""
echo "========================================"
echo "Framework Check After Merge"
echo "========================================"
echo ""

# Run framework check
if [[ -x "./ci/enforce_framework.sh" ]]; then
    ./ci/enforce_framework.sh
fi

echo ""
echo "Merge complete. Remember to update documentation if needed."
echo ""
EOF
chmod +x "$PROJECT_ROOT/.git/hooks/post-merge"
echo "✓ post-merge hook installed"

echo ""
echo "====================================="
echo "Git Hooks Installation Complete"
echo "====================================="
echo ""
echo "Installed hooks:"
echo "  • pre-commit    - Framework compliance check"
echo "  • commit-msg    - Message format validation"
echo "  • post-checkout - Branch switch reminder"
echo "  • post-merge    - Post-merge framework check"
echo ""
echo "To bypass hooks temporarily:"
echo "  git commit --no-verify"
echo ""
echo "To uninstall:"
echo "  rm .git/hooks/pre-commit .git/hooks/commit-msg .git/hooks/post-checkout .git/hooks/post-merge"
echo ""
