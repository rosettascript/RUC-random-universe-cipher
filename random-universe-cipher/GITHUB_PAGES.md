# GitHub Pages Deployment

This project is configured to automatically deploy to GitHub Pages using GitHub Actions.

## Setup Instructions

### 1. Enable GitHub Pages

1. Go to your repository on GitHub
2. Navigate to **Settings** → **Pages**
3. Under **Source**, select **GitHub Actions**
4. Save the settings

### 2. Repository Name Configuration

The deployment workflow automatically detects your repository name and sets the base path. If your repository is `username/random-universe`, the site will be available at:

```
https://username.github.io/random-universe/
```

### 3. Automatic Deployment

The workflow automatically:
- ✅ Builds C++ WASM using Emscripten
- ✅ Builds TypeScript/Vite project
- ✅ Copies WASM files to dist folder
- ✅ Deploys to GitHub Pages

**Trigger**: Pushes to `main` or `master` branch, or manual workflow dispatch

## Manual Deployment

If you want to test the build locally with GitHub Pages base path:

```bash
# Set your repository name
export BASE_PATH="/your-repo-name/"

# Build C++ WASM
npm run build:cpp-wasm

# Build project
npm run build

# Copy WASM files
mkdir -p dist/cpp-wasm/pkg
cp cpp-wasm/pkg/*.js dist/cpp-wasm/pkg/
cp cpp-wasm/pkg/*.wasm dist/cpp-wasm/pkg/

# Preview (will use base path)
npm run preview
```

## Local Development

For local development (without base path):

```bash
# Just run dev server (no base path)
npm run dev
```

## Troubleshooting

### WASM Files Not Loading

If WASM files fail to load on GitHub Pages:

1. Check that `.nojekyll` file exists in the root
2. Verify WASM files are in `dist/cpp-wasm/pkg/` after build
3. Check browser console for 404 errors
4. Ensure base path matches your repository name

### Build Fails

1. Check GitHub Actions logs
2. Verify Emscripten setup in workflow
3. Ensure all dependencies are in `package.json`

### Wrong Base Path

If assets aren't loading, the base path might be incorrect:

1. Check repository name in GitHub Actions workflow
2. Verify `BASE_PATH` environment variable in build step
3. Check `vite.config.ts` base configuration

## File Structure

After deployment, the site structure will be:

```
https://username.github.io/repo-name/
├── index.html
├── assets/
│   ├── index-[hash].js
│   ├── index-[hash].css
│   └── ...
└── cpp-wasm/
    └── pkg/
        ├── ruc_wasm.js
        └── ruc_wasm.wasm
```

## Custom Domain

To use a custom domain:

1. Add `CNAME` file to repository root with your domain
2. Configure DNS records as per GitHub Pages documentation
3. Update `vite.config.ts` base path to `/` (root)

## Workflow Details

The deployment workflow (`.github/workflows/deploy.yml`) performs:

1. **Checkout** - Gets repository code
2. **Setup Node.js** - Installs Node.js 20 with npm cache
3. **Setup Emscripten** - Installs Emscripten SDK for C++ WASM
4. **Install Dependencies** - Runs `npm ci`
5. **Build C++ WASM** - Compiles C++ to WebAssembly
6. **Get Repository Name** - Extracts repo name for base path
7. **Build Project** - Runs Vite build with correct base path
8. **Copy WASM Files** - Ensures WASM files are in dist
9. **Deploy** - Uploads to GitHub Pages

## Performance

The deployed site includes:
- ✅ Optimized C++ WASM (5.0x faster than JS)
- ✅ Parallel processing support
- ✅ All optimizations enabled
- ✅ Source maps for debugging

## Security

⚠️ **Note**: This is a demo implementation. For production use:
- Review security considerations
- Consider third-party security audit
- Implement additional security measures

