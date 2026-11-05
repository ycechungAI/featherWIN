const saved = document.getElementById("saved");

chrome.storage.local.get("testValue").then((result) => {
  if (result.testValue) {
    saved.textContent = result.testValue;
  }
});

document.getElementById("save").addEventListener("click", async () => {
  const value = "saved at " + new Date().toISOString();
  await chrome.storage.local.set({ testValue: value });
  saved.textContent = value;
});
