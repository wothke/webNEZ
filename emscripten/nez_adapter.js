/*
 nez_adapter.js: Adapts NEZplug++ backend to generic WebAudio/ScriptProcessor player.
 
 version 1.0
 
 	Copyright (C) 2018 Juergen Wothke

 LICENSE
 
 This library is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2.1 of the License, or (at
 your option) any later version. This library is distributed in the hope
 that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/
NEZBackendAdapter = (function(){ var $this = function () {
		$this.base.call(this, backend_NEZ.Module, 2);
		this._manualSetupComplete= true;
		this._undefined;
		this._currentPath;
		this._currentFile;
		
		if (!backend_NEZ.Module.notReady) {
			// in sync scenario the "onRuntimeInitialized" has already fired before execution gets here,
			// i.e. it has to be called explicitly here (in async scenario "onRuntimeInitialized" will trigger
			// the call directly)
			this.doOnAdapterReady();
		}				
	}; 
	// sample buffer contains 2-byte integer sample data (i.e. 
	// must be rescaled) of 2 interleaved channels
	extend(EmsHEAP16BackendAdapter, $this, {
		doOnAdapterReady: function() {
			// called when runtime is ready (e.g. asynchronously when WASM is loaded)
			// if FS needed to be setup of would be done here..
		},
		getAudioBuffer: function() {
			var ptr=  this.Module.ccall('emu_get_audio_buffer', 'number');			
			// make it a this.Module.HEAP16 pointer
			return ptr >> 1;	// 2 x 16 bit samples			
		},
		getAudioBufferLength: function() {
			var len= this.Module.ccall('emu_get_audio_buffer_length', 'number');
			return len;
		},
		computeAudioSamples: function() {
			return this.Module.ccall('emu_compute_audio_samples', 'number');
		},
		getMaxPlaybackPosition: function() { 
			return this.Module.ccall('emu_get_max_position', 'number');
		},
		getPlaybackPosition: function() {
			return this.Module.ccall('emu_get_current_position', 'number');
		},
		seekPlaybackPosition: function(pos) {
			this.Module.ccall('emu_seek_position', 'number', ['number'], [pos]);
		},		
		getPathAndFilename: function(filename) {
			var sp = filename.split('/');
			var fn = sp[sp.length-1];					
			var path= filename.substring(0, filename.lastIndexOf("/"));	
			if (path.lenght) path= path+"/";
			
			return [path, fn];
		},
		mapBackendFilename: function (name) {
			// "name" comes from the C++ side 
			var input= this.Module.Pointer_stringify(name);
			return input;
		},
		registerFileData: function(pathFilenameArray, data) {
			return this.registerEmscriptenFileData(pathFilenameArray, data);
		},
		loadMusicData: function(sampleRate, path, filename, data, options) {
			var buf = this.Module._malloc(data.length);
			this.Module.HEAPU8.set(data, buf);
			var ret = this.Module.ccall('emu_load_file', 'number', ['string', 'number', 'number'], [filename, buf, data.length]);
			this.Module._free(buf);

			if (ret == 0) {
				this.playerSampleRate = this.Module.ccall('emu_get_sample_rate', 'number');
				this.resetSampleRate(sampleRate, this.playerSampleRate);
				this._currentPath= path;
				this._currentFile= filename;
			} else {
				this._currentPath= this._undefined;
				this._currentFile= this._undefined;
			}
			return ret;			
		},
		evalTrackOptions: function(options) {
			if (typeof options.timeout != 'undefined') {
				ScriptNodePlayer.getInstance().setPlaybackTimeout(options.timeout*1000);
			} else {
				ScriptNodePlayer.getInstance().setPlaybackTimeout(-1);	// reset last songs setting
			}
			var id= (options && options.track) ? options.track : -1;	// by default do not set track		
			var boostVolume= (options && options.boostVolume) ? options.boostVolume : 0;		
			return this.Module.ccall('emu_set_subsong', 'number', ['number', 'number'], [id, boostVolume]);
		},				
		teardown: function() {
			this.Module.ccall('emu_teardown', 'number');	// just in case
		},
		getSongInfoMeta: function() {
			return {title: String,
					track: String, 
					artist: String, 
					copyright: String, 
					};
		},
		
		updateSongInfo: function(filename, result) {
			var numAttr= 4;
			var ret = this.Module.ccall('emu_get_track_info', 'number');

			var array = this.Module.HEAP32.subarray(ret>>2, (ret>>2)+numAttr);
			result.title= this.Module.Pointer_stringify(array[0]);
			result.track= this.Module.Pointer_stringify(array[1]);		
			result.artist= this.Module.Pointer_stringify(array[2]);		
			result.copyright= this.Module.Pointer_stringify(array[3]);		
		}
	});	return $this; })();