// Tells the page when a new service worker takes over (a deploy), but not on the first install.
export function watchUpdates(sw: Pick<ServiceWorkerContainer, 'controller' | 'addEventListener'>, onUpdate: () => void): void {
  if (!sw.controller) return; // first visit: the worker claiming the page is not an update
  let told = false;
  sw.addEventListener('controllerchange', () => {
    if (told) return;
    told = true;
    onUpdate();
  });
}
