Module.preRun = Module.preRun || [];
Module.preRun.push(function() {
  FS.mkdirTree("/persist");
  FS.mount(IDBFS, {}, "/persist");

  addRunDependency("lexi-persist");
  FS.syncfs(true, function(err) {
    if (err) {
      console.error("FS.syncfs(true) failed", err);
    }
    removeRunDependency("lexi-persist");
  });
});
