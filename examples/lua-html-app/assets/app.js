// lua-html-app client-side logic — served via coconut://assets/app.js
let count = 0;
const counter = document.getElementById('counter');
const incBtn = document.getElementById('inc-btn');
if (incBtn) {
  incBtn.addEventListener('click', () => {
    counter.textContent = ++count;
  });
}
