# Local patches of the submodules

`lib/ZenKit` is the upstream GothicKit/ZenKit, OpenGothic needs three changes that are not in it yet:

- `DaedalusVm::invoke_function` - a call of a function with the arguments already on the stack (`AI_Function`, `debug.call`);
- `DaedalusVm::pop_int` accepts an instance and returns the index of its symbol, as the original engine does (the destructor
  `Bar_Delete(var int)` of LeGo is called with an instance);
- `INpc::compat_focus_vob` / `compat_interact_mob` - the members of oCNpc that C_NPC does not have.
- `oCNpcEx` / `oCItemEx` of Union (saves of New Balance and other mods) are read as `oCNpc` / `oCItem`: the import of original saves (`docs/ORYGINALNE_ZAPISY.md`).

CMake applies the patch by itself (`cmake/zenkitpatch.cmake`, before ZenKit is added): a clean clone, the Android build and the CI need nothing more. A tree that has it already is left alone. By hand (the patch fails when it was applied already):

```bash
git -C lib/ZenKit apply --ignore-whitespace ../../patches/zenkit-opengothic.patch
```

Make the patch again after a change in the submodule: `git -C lib/ZenKit diff > patches/zenkit-opengothic.patch`.

## Tempest

`lib/Tempest` is the upstream Try/Tempest. `patches/tempest-opengothic.patch` is about the creation of the Vulkan pipelines, a crash on Mali (Pixel 9, `hardened_malloc: detected write after free` in `vkCreateComputePipelines`):

- `VCompPipeline` destroyed the pipeline layout when the driver refused the pipeline, but the layout belongs to `VPsoLayoutCache` and the other pipelines use it: it was freed and handed out again;
- no pipeline derivatives (`ALLOW_DERIVATIVES`, `DERIVATIVE_BIT`): the bindless variants (update-after-bind) were derived from a base pipeline, Arm advises against it and it brings nothing;
- `VDevice::pipelineSync`: one pipeline is compiled at a time (the compilation of the shaders runs on a thread of its own while the others create objects).

A second change is about null descriptors (a crash of the intro on Mali-G720, `vkUpdateDescriptorSets` in `VDescriptorArray::populate`): `DrawBuckets::updateBindlessArrays` rounds the arrays up to 1024 and the rest is `nullptr`, and Tempest wrote `VK_NULL_HANDLE` for them. That is allowed with the feature `nullDescriptor` of VK_EXT_robustness2 only, but `hasRobustness2` was set by the presence of the extension. Now `props.nullDescriptor` is the feature itself, and without it the empty slots (of textures and of buffers) get a real 1x1 image (`VDevice::dummyTexture`) or the dummy buffer.

The third change is in `Engine/cmake/TempestAndroid.cmake`: the generated Android project gets a `gradle.properties` with `android.useAndroidX=true`, because OkHttp (a dependency of the Steam login) brings AndroidX libraries and AGP refuses to build without it.

CMake applies it (`cmake/tempestpatch.cmake`, before Tempest is added), as for ZenKit. By hand: `git -C lib/Tempest apply --ignore-whitespace ../../patches/tempest-opengothic.patch`; make it again with `git -C lib/Tempest diff > patches/tempest-opengothic.patch`.

The fourth change is in `Engine/system/api/androidapi.cpp/.h` and `Engine/gapi/vulkan/vswapchain.cpp/.h` (NB fix 73): the game survives the background of Android (a message, another application, the home screen). Before, `APP_CMD_TERM_WINDOW` closed the game (and it crashed in `MainWindow::tick` while closing). Now the window of Tempest is a tag and the native window behind it changes: it goes away in the background (nothing is drawn), `APP_CMD_INIT_WINDOW` gives a new one and the resize event makes the swapchain anew; `VSwapchain` makes its surface on `AndroidApi::nativeWindow()` and a new surface when `AndroidApi::nativeWindowGeneration()` changed. Only `APP_CMD_DESTROY` closes the game.
