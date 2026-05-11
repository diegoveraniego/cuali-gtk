const { invoke } = window.__TAURI__.core;

async function waitForServer() {
  const isUp = await invoke("check_server");
  if (isUp) {
    const url = await invoke("get_server_url");
    console.log("Redirecting to", url);
    window.location.href = url;
  } else {
    setTimeout(waitForServer, 500);
  }
}

window.addEventListener("DOMContentLoaded", () => {
  waitForServer();
});
