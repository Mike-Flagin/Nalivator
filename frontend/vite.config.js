import {defineConfig} from 'vite'
import react from '@vitejs/plugin-react'
import compression  from 'vite-plugin-compression2'
import fs from 'fs'
import path from 'path'

function cleanDirExcept(dirPath, keepFiles) {
    if (!fs.existsSync(dirPath)) return

    const items = fs.readdirSync(dirPath)
    for (const item of items) {
        if (keepFiles.includes(item)) continue

        const fullPath = path.join(dirPath, item)
        fs.rmSync(fullPath, {recursive: true, force: true})
    }
}

function selectiveCleanPlugin(targetDir, ignoreList) {
    return {
        name: 'selective-clean',
        buildStart() {
            cleanDirExcept(path.resolve(__dirname, targetDir), ignoreList)
        }
    }
}

// https://vite.dev/config/
export default defineConfig(({mode}) => {
    const isEsp = mode === 'esp'
    return {
        base: './',
        build: {
            outDir: isEsp ? '../data' : 'dist',
            emptyOutDir: !isEsp
        },
        plugins: [
            react(),
            isEsp && selectiveCleanPlugin('../data', ['config.json', 'config_default.json', 'ingredients.json', 'recipes.json']),
            isEsp && compression ({
                algorithms: ['gzip'],
                deleteOriginalAssets: true
            }),
        ].filter(Boolean),
    }
})
