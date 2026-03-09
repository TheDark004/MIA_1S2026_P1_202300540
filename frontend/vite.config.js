import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    port: 3000,   // frontend en localhost:3000
    // Proxy -> redirige /api/* al backend C++ en puerto 18080
    
    proxy: {
      "/execute": "http://localhost:18080",
      "/health":  "http://localhost:18080",
    }
  }
})
