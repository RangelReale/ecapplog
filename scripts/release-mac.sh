#!/usr/bin/env bash
#
# Publishes a signed and notarized macOS DMG to GitHub Releases.
#
# The verification step is a hard gate, not a warning: an un-notarized DMG installs perfectly on
# the machine that built it and is blocked by Gatekeeper everywhere else, so a broken release
# looks fine locally right up until someone else downloads it.
#
# Usually invoked as "make release" from the build directory.

set -euo pipefail

BUILD_DIR="build-release"
DMG=""
VERSION=""
TAG=""
NOTES_FILE=""
DRAFT=1
ASSUME_YES=0
VERIFY_ONLY=0

usage() {
    cat <<'EOF'
Usage: scripts/release-mac.sh [options]

  --build-dir DIR   directory to look for the DMG in (default: build-release)
  --dmg PATH        explicit path to the DMG (default: newest *.dmg in the build dir)
  --version X.Y.Z   release version (default: parsed from the DMG file name)
  --tag TAG         git tag to release (default: v<version>)
  --notes-file PATH markdown file to use as the release notes (default: a generic one-liner)
  --publish         publish immediately instead of creating a draft
  --verify-only     run the notarization checks and stop
  -y, --yes         do not prompt before tagging, pushing and uploading
  -h, --help        show this help
EOF
}

die() { printf 'error: %s\n' "$*" >&2; exit 1; }
info() { printf '==> %s\n' "$*"; }
indent() { sed 's/^/    /'; }

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) BUILD_DIR="${2:?missing value}"; shift 2 ;;
        --dmg)       DMG="${2:?missing value}"; shift 2 ;;
        --version)   VERSION="${2:?missing value}"; shift 2 ;;
        --tag)       TAG="${2:?missing value}"; shift 2 ;;
        --notes-file) NOTES_FILE="${2:?missing value}"; shift 2 ;;
        --publish)   DRAFT=0; shift ;;
        --verify-only) VERIFY_ONLY=1; shift ;;
        -y|--yes)    ASSUME_YES=1; shift ;;
        -h|--help)   usage; exit 0 ;;
        *) usage >&2; die "unknown argument: $1" ;;
    esac
done

[ "$(uname -s)" = "Darwin" ] || die "this script only runs on macOS"

# Checked up front rather than at upload time: a missing notes file must not surface after the tag
# has already been created and pushed.
if [ -n "$NOTES_FILE" ] && [ ! -f "$NOTES_FILE" ]; then
    die "no such notes file: $NOTES_FILE"
fi

#
# Locate the package
#
if [ -z "$DMG" ]; then
    # Newest first, so a stale DMG from an earlier version is never picked up silently.
    DMG=$(ls -t "$BUILD_DIR"/*.dmg 2>/dev/null | head -1 || true)
    [ -n "$DMG" ] || die "no .dmg found in '$BUILD_DIR' — run cpack first"
fi
[ -f "$DMG" ] || die "no such file: $DMG"
info "package: $DMG"

#
# Verification gate
#
info "checking code signature"
codesign --verify --verbose=2 "$DMG" 2>&1 | indent \
    || die "the DMG is not validly signed"

info "checking notarization ticket"
if ! staple_out=$(xcrun stapler validate "$DMG" 2>&1); then
    printf '%s\n' "$staple_out" | indent >&2
    die "no notarization ticket is stapled to $(basename "$DMG").
Rebuild with -DECAPPLOG_NOTARIZE=ON and re-run cpack. Publishing this file would
reproduce the exact bug this gate exists to prevent: it works here and nowhere else."
fi
printf '%s\n' "$staple_out" | indent

info "checking Gatekeeper verdict"
if ! spctl_out=$(spctl -a -vvv -t install "$DMG" 2>&1); then
    printf '%s\n' "$spctl_out" | indent >&2
    die "Gatekeeper rejected $(basename "$DMG")"
fi
printf '%s\n' "$spctl_out" | indent
case "$spctl_out" in
    *"source=Notarized Developer ID"*) ;;
    *) die "expected 'source=Notarized Developer ID' in the verdict above" ;;
esac

#
# Version and tag
#
if [ -z "$VERSION" ]; then
    VERSION=$(basename "$DMG" | sed -nE 's/^[A-Za-z]+-([0-9]+\.[0-9]+\.[0-9]+).*/\1/p')
    [ -n "$VERSION" ] || die "could not parse a version from '$(basename "$DMG")' — pass --version"
fi
if [ -z "$TAG" ]; then
    TAG="v$VERSION"
fi

if [ "$VERIFY_ONLY" -eq 1 ]; then
    info "verification passed for $VERSION ($TAG) — stopping, nothing published"
    exit 0
fi

#
# Publishing preconditions
#
command -v gh >/dev/null 2>&1 \
    || die "gh is not installed — run: brew install gh && gh auth login"
gh auth status >/dev/null 2>&1 \
    || die "gh is not authenticated — run: gh auth login"

if ! git diff --quiet || ! git diff --cached --quiet; then
    die "working tree has uncommitted changes — commit them before releasing"
fi

remote_url=$(git remote get-url origin)
repo=$(printf '%s' "$remote_url" | sed -E 's#^(git@github\.com:|https://github\.com/)##; s#\.git$##')
[ -n "$repo" ] || die "could not derive owner/repo from remote '$remote_url'"

# The branch has to reach origin as well as the tag. Pushing only the tag leaves the release
# pointing at a commit that is not reachable from the branch, so the version bump never shows up in
# the repository history.
branch=$(git rev-parse --abbrev-ref HEAD)
[ "$branch" != "HEAD" ] || die "detached HEAD — check out a branch before releasing"
git rev-parse --abbrev-ref --symbolic-full-name '@{upstream}' >/dev/null 2>&1 \
    || die "branch '$branch' has no upstream — push it once with: git push -u origin $branch"
unpushed=$(git rev-list --count '@{upstream}'..HEAD)

create_tag=1
if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
    if [ "$(git rev-list -n1 "$TAG")" != "$(git rev-parse HEAD)" ]; then
        die "tag $TAG already exists but does not point at HEAD"
    fi
    info "tag $TAG already exists at HEAD"
    create_tag=0
fi

if gh release view "$TAG" --repo "$repo" >/dev/null 2>&1; then
    die "release $TAG already exists on $repo — bump the version or delete it first"
fi

#
# Confirm, then publish
#
if [ "$ASSUME_YES" -eq 0 ]; then
    tag_note=""
    if [ "$create_tag" -eq 1 ]; then
        tag_note=" (will be created)"
    fi
    state="draft"
    if [ "$DRAFT" -eq 0 ]; then
        state="published immediately"
    fi
    printf '\n'
    branch_note="already up to date with origin"
    if [ "$unpushed" -gt 0 ]; then
        branch_note="$unpushed commit(s) will be pushed"
    fi
    printf 'About to release to %s:\n' "$repo"
    printf '  branch : %s (%s)\n' "$branch" "$branch_note"
    printf '  tag    : %s%s, pushed to origin\n' "$TAG" "$tag_note"
    printf '  asset  : %s\n' "$DMG"
    printf '  state  : %s\n' "$state"
    printf '\nContinue? [y/N] '
    read -r reply
    case "$reply" in
        [yY]|[yY][eE][sS]) ;;
        *) die "aborted" ;;
    esac
fi

# Branch before tag, deliberately: if the branch push is rejected (non-fast-forward, no
# permission) the release aborts here, rather than after a tag has already reached origin.
if [ "$unpushed" -gt 0 ]; then
    info "pushing $branch to origin ($unpushed commit(s))"
    git push origin "$branch"
fi

if [ "$create_tag" -eq 1 ]; then
    info "creating tag $TAG"
    git tag -a "$TAG" -m "ECAppLog $VERSION"
fi

info "pushing $TAG to origin"
git push origin "$TAG"

release_args=(
    --repo "$repo"
    --title "ECAppLog $VERSION"
)
if [ -n "$NOTES_FILE" ]; then
    release_args+=(--notes-file "$NOTES_FILE")
else
    release_args+=(--notes "Signed and notarized macOS build. Requires macOS 11 (Big Sur) or later.")
fi
if [ "$DRAFT" -eq 1 ]; then
    release_args+=(--draft)
fi

info "creating release $TAG"
gh release create "$TAG" "${release_args[@]}" "$DMG"

if [ "$DRAFT" -eq 1 ]; then
    info "draft created — review it, then publish with:"
    printf '        gh release edit %s --repo %s --draft=false\n' "$TAG" "$repo"
else
    info "released $TAG"
fi
