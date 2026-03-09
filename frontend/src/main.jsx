import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'

import App from './App.jsx'

document.body.style.margin          = "0"
document.body.style.padding         = "0"
document.body.style.backgroundColor = "#0d1117"
document.body.style.boxSizing       = "border-box"

createRoot(document.getElementById('root')).render(
  <StrictMode>
    <App />
  </StrictMode>,
)
