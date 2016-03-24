#include <Windows.h>

#include <stdio.h>
#include <stdlib.h>

//includi qui il tuo apikey file
#include <api.h>

#include <lame.h>

#include "wav_file.h"
#include <math.h>

void __stdcall logged_in (sp_session *session, sp_error error);
void __stdcall logged_out (sp_session *session);
void __stdcall metadata_updated (sp_session *session);
void __stdcall connection_error (sp_session *session, sp_error error);
void __stdcall message_to_user (sp_session *session, const char *message);
void __stdcall notify_main_thread (sp_session *session);
int __stdcall  music_delivery (sp_session *session, const sp_audioformat *format, const void *frames, int num_frames);
void __stdcall play_token_lost (sp_session *session);
void __stdcall log_message (sp_session *session, const char *data);
void __stdcall end_of_track (sp_session *session);
void __stdcall streaming_error (sp_session *session, sp_error error);
void __stdcall userinfo_updated (sp_session *session);
void __stdcall start_playback (sp_session *session);
void __stdcall stop_playback (sp_session *session);
void __stdcall get_audio_buffer_stats (sp_session *session, sp_audio_buffer_stats *stats);
void __stdcall offline_status_updated (sp_session *session);
void __stdcall offline_error (sp_session *session, sp_error error);
void __stdcall credentials_blob_updated (sp_session *session, const char *blob);
void __stdcall connectionstate_updated (sp_session *session);
void __stdcall scrobble_error (sp_session *session, sp_error error);

void __stdcall search_complete (sp_search *result, void *userdata);
void __stdcall albumbrowse_complete(sp_albumbrowse *result, void *userdata);


void __stdcall playlist_added(sp_playlistcontainer *pc, sp_playlist *playlist, int position, void *userdata);
void __stdcall playlist_removed(sp_playlistcontainer *pc, sp_playlist *playlist, int position, void *userdata);
void __stdcall playlist_moved(sp_playlistcontainer *pc, sp_playlist *playlist, int position, int new_position, void *userdata);
void __stdcall container_loaded(sp_playlistcontainer *pc, void *userdata);

void __stdcall tracks_added(sp_playlist *pl, sp_track *const *tracks, int num_tracks, int position, void *userdata);
void __stdcall tracks_removed(sp_playlist *pl, const int *tracks, int num_tracks, void *userdata);
void __stdcall tracks_moved(sp_playlist *pl, const int *tracks, int num_tracks, int new_position, void *userdata);
void __stdcall playlist_renamed(sp_playlist *pl, void *userdata);
void __stdcall playlist_state_changed(sp_playlist *pl, void *userdata);
void __stdcall playlist_update_in_progress(sp_playlist *pl, bool done, void *userdata);
void __stdcall playlist_metadata_updated(sp_playlist *pl, void *userdata);
void __stdcall track_created_changed(sp_playlist *pl, int position, sp_user *user, int when, void *userdata);
void __stdcall track_seen_changed(sp_playlist *pl, int position, bool seen, void *userdata);
void __stdcall description_changed(sp_playlist *pl, const char *desc, void *userdata);
void __stdcall image_changed(sp_playlist *pl, const byte *image, void *userdata);
void __stdcall track_message_changed(sp_playlist *pl, int position, const char *message, void *userdata);
void __stdcall subscribers_changed(sp_playlist *pl, void *userdata);


struct DownloadItem
{

	sp_link *spotify_link;
	sp_search *spotify_search;

	int start_loading;

	DownloadItem *next_item;

	void *spotify_userdata;
};

struct SpotifyUserData
{
	CRITICAL_SECTION	spotify_lock;
	CONDITION_VARIABLE	spotify_cond;
	CONDITION_VARIABLE	spotify_close_cond;

	int spotify_logged_in;

	int					spotify_flag;
	int					spotify_close_flag;

	HWND				spotify_window;

	int spotify_timeout;
	int spotify_active;

	sp_session	*spotify;
	sp_playlistcontainer *container;
	sp_track *track;
	
	sp_search *search;
	sp_albumbrowse *album_browse;

	int actual_download_album;
	int actual_download_track;
	char actual_dir_name [1024];

	//cache nomi
	const char *actual_download_album_name;
	const char *actual_download_track_name;
	int actual_download_track_is_single;
	int actual_samples;
	int track_total_samples;
	int last_written_samples;

	int metadata_loaded;

	CRITICAL_SECTION	spotify_buffer_lock;
	CONDITION_VARIABLE	spotify_buffer_cond;
	
	int last_bufferoverrun;
	
	WaveFile *actual_file;

	lame_global_flags *lame;
	void *encoder_buffer;
	int mp3_buffersize;
	FILE *fp_mp3;
	int encoder_ready;
	int track_downloaded;

	DownloadItem *download_list;
	DownloadItem *actual_download_item;

};

sp_playlistcontainer_callbacks container_cb = {
	playlist_added,
	playlist_removed,	
	playlist_moved,
	container_loaded
};

sp_playlist_callbacks playlist_cb = {
	tracks_added,
	tracks_removed,
	tracks_moved,
	playlist_renamed,
	playlist_state_changed,	
	playlist_update_in_progress,
	playlist_metadata_updated,
	track_created_changed,
	track_seen_changed,
	description_changed,
	image_changed,
	track_message_changed,
	subscribers_changed
};



extern "C"
{
	FILE __iob_func[3] = { *stdin,*stdout,*stderr };
}

void track_ended(SpotifyUserData *context);

int SpotifyLogOut ( SpotifyUserData *instance );
int SpotifyLogIn ( SpotifyUserData *instance, char *username, char *password );

LRESULT CALLBACK SpotifyWndProc ( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI SpotifyMainProc ( void *data );
void SpotifyMainLoop ( SpotifyUserData *data );

void process_download_list ( SpotifyUserData *userdata, char *filename );

LRESULT SpotifyHandleMessage ( UINT msg, WPARAM wparam, LPARAM lparam, SpotifyUserData *data );

int PrepareEncoder ( SpotifyUserData *context, const sp_audioformat *fmt );
int CloseEncoder ( SpotifyUserData *context );
int FreeMP3Buffer ( SpotifyUserData *context );
int AllocMp3Buffer ( SpotifyUserData *context, int num_frames );

int main()
{

	int  spotify_logged_in = 0;
    char *username = 0;
    char *password = 0;
 
	sp_session_config spotify_config;
	sp_session_callbacks spotify_session_cb;

	HANDLE		spotify_main_thread;
	DWORD		spotify_main_thread_id;
	WNDCLASSEX	message_class;
	HWND	spotify_hwnd;

    SpotifyUserData *user_data = new SpotifyUserData;
    memset(user_data, 0, sizeof(SpotifyUserData));
 
    user_data->spotify_flag = 0;
    InitializeCriticalSection(&user_data->spotify_lock);
    InitializeConditionVariable(&user_data->spotify_cond);
    user_data->spotify_timeout = 0;
	user_data->spotify_active = 0;

    InitializeCriticalSection(&user_data->spotify_buffer_lock);
    InitializeConditionVariable(&user_data->spotify_buffer_cond);
    InitializeConditionVariable(&user_data->spotify_close_cond);
 
    //creo una finestra soli messaggi per ricevere i messaggi da spotify
    //registro la classe
 
    memset(&message_class, 0, sizeof(WNDCLASSEX));
    message_class.cbSize = sizeof(WNDCLASSEX);
    message_class.lpfnWndProc = SpotifyWndProc;
    message_class.hInstance = GetModuleHandle(NULL);
    message_class.lpszClassName = MESSAGE_WINDOW_CLASS_NAME_SPOTIFY;
 
    if ( RegisterClassEx(&message_class) ) 
    {
        spotify_hwnd = CreateWindowEx( 0, MESSAGE_WINDOW_CLASS_NAME_SPOTIFY, MESSAGE_WINDOW_CLASS_NAME_SPOTIFY, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, user_data );
    }

    user_data->spotify_window = spotify_hwnd;
 
    //creo la sessione spotify
    memset(&spotify_config, 0, sizeof(sp_session_config));
    memset(&spotify_session_cb, 0, sizeof(sp_session_callbacks));
 
    spotify_session_cb.connection_error     = connection_error;
    spotify_session_cb.logged_in            = logged_in;
    spotify_session_cb.logged_out           = logged_out;
    spotify_session_cb.log_message          = log_message;
    spotify_session_cb.music_delivery       = music_delivery;
    spotify_session_cb.notify_main_thread   = notify_main_thread;
    spotify_session_cb.start_playback       = start_playback;
    spotify_session_cb.stop_playback        = stop_playback;
    spotify_session_cb.end_of_track         = end_of_track;
	spotify_session_cb.get_audio_buffer_stats = get_audio_buffer_stats;

    spotify_config.api_version = SPOTIFY_API_VERSION;
    spotify_config.application_key = g_appkey;
    spotify_config.application_key_size = g_appkey_size;
    spotify_config.cache_location = "spotify_cache";
    spotify_config.settings_location = "spotify_cache";
    spotify_config.userdata = (void *)user_data;
    spotify_config.callbacks = &spotify_session_cb;
    spotify_config.user_agent = "SpotifyApp";

	user_data->lame = lame_init();
	user_data->encoder_ready = 0;

	AllocMp3Buffer(user_data, 0);

    sp_error err = sp_session_create(&spotify_config, &user_data->spotify);

    if ( SP_ERROR == err )
    {
        //error handling
        user_data->spotify = NULL;
    }
    else
    {
        
        user_data->spotify_active = 1;
        spotify_main_thread = CreateThread(NULL, 0, SpotifyMainProc, user_data, 0, &spotify_main_thread_id);

		SpotifyLogIn(user_data, USERNAME, PASSWORD);
    }


	/////////////////// LOOP MESSAGGI
	MSG msg;
	BOOL bRet;
 
	while((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if(bRet == -1)
		{
		// Handle Error
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}


void process_download_list ( SpotifyUserData *userdata, char *filename )
{

	userdata->download_list = 0;

	FILE *fp = fopen(filename, "rb");

	if ( !fp ) return ;

	char tmp_link[1000];
	memset(tmp_link, 0, 1000);


	//EnterCriticalSection(&userdata->spotify_lock);

	DownloadItem *actual_pos = userdata->download_list;

	while ( fgets(tmp_link, 999, fp) != NULL )
	{
		int len = strlen(tmp_link);

		if ( len <= 2 ) continue;
		
		if ( tmp_link[len-2] == '\r' && tmp_link[len-1] == '\n' ) tmp_link[len-2] = 0;
		
		sp_link *link = sp_link_create_from_string(tmp_link);

		if ( !link ) continue;

		DownloadItem *item = (DownloadItem *)malloc(sizeof(DownloadItem));
		memset(item, 0, sizeof(DownloadItem));

		item->start_loading = 0;
		item->next_item = 0;
		item->spotify_link = link;
		item->spotify_search = 0;
		item->spotify_userdata = userdata;

		//if (sp_link_type(link) == SP_LINKTYPE_ALBUM) sp_album_name(sp_link_as_album(link));
		//if (sp_link_type(link) == SP_LINKTYPE_TRACK) sp_track_add_ref(sp_link_as_track(link));

		if ( !actual_pos )
		{
			userdata->download_list = item;
			actual_pos = item;
		}
		else
		{
			actual_pos->next_item = item;
			actual_pos = item;
		}

		//printf("Lancio ricerca per link: %s\r\n", tmp_link);
		//sp_search *tmp = sp_search_create(userdata->spotify, tmp_link, 0, 1, 0, 1, 0, 1, 0, 1, SP_SEARCH_STANDARD, search_complete, item);

	}

	//WakeAllConditionVariable(&userdata->spotify_cond);

	//LeaveCriticalSection(&userdata->spotify_lock);

}


DWORD WINAPI SpotifyMainProc ( void *data )
{
	SpotifyUserData *instance = (SpotifyUserData *)data;

	if ( instance )
	{
		SpotifyMainLoop(instance);
	}

	return (DWORD)-1;
}

void SpotifyMainLoop ( SpotifyUserData *data )
{
	//avvio il loop
	
	while ( data->spotify_active )
	{
		EnterCriticalSection(&data->spotify_lock);
		
		if ( !data->spotify_timeout )
		{
			while ( !data->spotify_flag )
			{
				SleepConditionVariableCS(&data->spotify_cond, &data->spotify_lock, INFINITE);
			}
		}
		else
		{
			SleepConditionVariableCS(&data->spotify_cond, &data->spotify_lock, data->spotify_timeout);
		}


		//guardia
		if ( !data->spotify_active )
		{
			data->spotify_close_flag = 1;
			WakeAllConditionVariable(&data->spotify_close_cond);
			LeaveCriticalSection(&data->spotify_lock);

			return ;
		}

		LeaveCriticalSection(&data->spotify_lock);

		do
		{
			sp_session_process_events(data->spotify, &data->spotify_timeout);
			//printf("Timeout: %i\r\n", data->spotify_timeout);
		}
		while ( !data->spotify_timeout );
	}
}


LRESULT CALLBACK SpotifyWndProc ( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//create
	LONG_PTR res = 0;

	if ( WM_CREATE == uMsg )
	{
		CREATESTRUCT *params = (CREATESTRUCT *)lParam;
		SetWindowLong(hwnd, GWLP_USERDATA, (LONG)params->lpCreateParams);
	}
	else if (uMsg >= SPOTIFY_LOGGED_IN && uMsg <= SPOTIFY_CLOSE_SINGLE_TRACK)
	{

		res = -1;

		SpotifyUserData *instance = (SpotifyUserData *)GetWindowLong(hwnd, GWLP_USERDATA);

		if ( instance )
		{
			//processing messaggi da spotify
			res = SpotifyHandleMessage (uMsg, wParam, lParam, instance );
		}

		if ( res >= 0 )
		{
			return res;
		}
	}
	
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT SpotifyHandleMessage ( UINT msg, WPARAM wparam, LPARAM lparam, SpotifyUserData *instance )
{
	//processo i messaggi spotify
	switch ( msg )
	{
		case SPOTIFY_LOGGED_IN:
		{
			instance->spotify_logged_in = 1;

			printf("SYSTEM MESSAGE: LOGGED IN\r\n");
			
			//process_download_list(instance, "downloads.txt");
			//PostMessage(instance->spotify_window, SPOTIFY_START_DOWNLOAD_LIST, 0, 0);

			//provo a scaricare le informazioni sulle playlist

			EnterCriticalSection(&instance->spotify_lock);

			instance->container = sp_session_playlistcontainer(instance->spotify);

			if ( instance->container ) sp_playlistcontainer_add_callbacks(instance->container, &container_cb, instance);

			LeaveCriticalSection(&instance->spotify_lock);
			
		}
		break;

		//case SPOTIFY_METADATA_UPDATED:
		case SPOTIFY_SEARCH_ITEM_COMPLETE:
		{



			EnterCriticalSection(&instance->spotify_lock);
	

			DownloadItem *p = (DownloadItem *)lparam;

			if (!p) return 0;

			sp_link *link = p->spotify_link;

			switch (sp_link_type(link))
			{
			case SP_LINKTYPE_ALBUM:
			{
				sp_album *album = sp_link_as_album(link);

				if (sp_album_is_loaded(album))
				{
					printf("Album: \"%s\" caricato!\r\n", sp_album_name(album));
				}
				else
				{
					printf("Album non ancora disponibile...\r\n");
				}
			}
				break;

			case SP_LINKTYPE_TRACK:
			{
				sp_track *track = sp_link_as_track(link);

				if (sp_track_is_loaded(track))
				{
					printf("Traccia: \"%s\" caricata!\r\n", sp_track_name(track));
				}
				else
				{
					printf("Traccia non ancora disponibile...\r\n");
				}
			}
				break;

			}

			/*DownloadItem *p = instance->download_list;



			while (p)
			{

				sp_link *link = p->spotify_link;

				switch (sp_link_type(link))
				{
					case SP_LINKTYPE_ALBUM:
					{
						sp_album *album = sp_link_as_album(link);

						if (sp_album_is_loaded(album))
						{
							printf("Album: \"%s\" caricato!\r\n", sp_album_name(album));
						}
						else
						{
							printf("Album non ancora disponibile...\r\n");
						}
					}
					break;	

					case SP_LINKTYPE_TRACK:
					{
						sp_track *track = sp_link_as_track(link);

						if (sp_track_is_loaded(track))
						{
							printf("Traccia: \"%s\" caricata!\r\n", sp_track_name(track));
						}
						else
						{
							printf("Album non ancora disponibile...\r\n");
						}
					}
					break;

				}

				p = p->next_item;

			}*/
			

			LeaveCriticalSection(&instance->spotify_lock);

		}
		break;

		case SPOTIFY_CLOSE_SINGLE_TRACK:
		{
			
			EnterCriticalSection(&instance->spotify_lock);

			sp_session_player_play(instance->spotify, false);

			track_ended(instance);

			LeaveCriticalSection(&instance->spotify_lock);
		}

		break;

		case SPOTIFY_METADATA_UPDATED:
		{
			EnterCriticalSection(&instance->spotify_lock);

			DownloadItem *p = instance->download_list;

			while (p)
			{

			sp_link *link = p->spotify_link;

			switch (sp_link_type(link))
			{
				case SP_LINKTYPE_ALBUM:
				{
					sp_album *album = sp_link_as_album(link);

					if (sp_album_is_loaded(album))
					{
						printf("Album: \"%s\" caricato!\r\n", sp_album_name(album));
					}
					else
					{
						printf("Album non ancora disponibile...\r\n");
					}
				}
				break;

				case SP_LINKTYPE_TRACK:
				{
					sp_track *track = sp_link_as_track(link);

					if (sp_track_is_loaded(track))
					{
						printf("Traccia: \"%s\" caricata!\r\n", sp_track_name(track));
					}
					else
					{
						printf("Album non ancora disponibile...\r\n");
					}
				}
				break;

			}

			p = p->next_item;

			}


			LeaveCriticalSection(&instance->spotify_lock);

		}
		break;

		case SPOTIFY_LOGGED_OUT:
		{
			instance->spotify_logged_in = 0;

			EnterCriticalSection(&instance->spotify_lock);
			
			instance->track = 0;
			instance->metadata_loaded = 0;
			instance->container = 0;

			printf("SYSTEM MESSAGE: LOGGED OUT\r\n");
			
			LeaveCriticalSection(&instance->spotify_lock);
			
		}
		break;

		case SPOTIFY_CLOSE_NUTS:
		{
			EnterCriticalSection(&instance->spotify_lock);
			
			instance->spotify_flag = 1;
			instance->spotify_timeout = 0;
			instance->spotify_active = 0;
			
			WakeAllConditionVariable(&instance->spotify_cond);
			

			LeaveCriticalSection(&instance->spotify_lock);
		}
		break;

		case SPOTIFY_LOG_MESSAGE:
		{
			char *msg = (char *)lparam;

			if ( msg ) 
			{
				//printf("MESSAGGIO: %s\r\n", msg);

				free(msg);
				msg = 0;

			}
		}
		break;

		case SPOTIFY_START_SEARCH:
		{
			printf("Starting Download....\r\n");
			
			EnterCriticalSection(&instance->spotify_lock);
			//avvio la ricerca delle canzoni
			sp_search_create(instance->spotify, "spotify:album:blablalba", 0, 100, 0, 100, 0, 100, 0, 100, SP_SEARCH_STANDARD, search_complete, instance);

			//avvio elaborazione elementi
		
			WakeAllConditionVariable(&instance->spotify_cond);

			LeaveCriticalSection(&instance->spotify_lock);
		}
		break;

		case SPOTIFY_SEARCH_COMPLETE:
		{
			printf("SYSTEM MESSAGE: SEARCH COMPLETE\r\n");

			int start_down = 0;
			instance->actual_download_album = 0;
			instance->actual_download_track = 0;

			EnterCriticalSection(&instance->spotify_lock);

			if ( instance->search )
			{

				int num_albums = sp_search_num_albums(instance->search);

				if ( num_albums > 0 )
				{
					//printf("Albums found: %i\r\n", num_albums);
					//avvio il download del primo album

					start_down = 1;


				}
			}

			WakeAllConditionVariable(&instance->spotify_cond);
			
			LeaveCriticalSection(&instance->spotify_lock);

			//notifico l'avvio del download dal primo album
			if ( start_down ) PostMessage(instance->spotify_window, SPOTIFY_BROWSE_ALBUM, (WPARAM)0, 0);

		}
		break;

		case SPOTIFY_START_DOWNLOAD_LIST:
		{
			printf("Starting Download....\r\n");
			
			EnterCriticalSection(&instance->spotify_lock);
			//avvio la ricerca delle canzoni
			
			//avvio elaborazione elementi

			if ( instance->download_list )
			{
				instance->actual_download_item = instance->download_list;

				PostMessage(instance->spotify_window, SPOTIFY_START_NEXT_DOWNLOAD, 0, 0);

			}
			else
			{
				printf("Nessun file in lista download......\r\n");
			}

			WakeAllConditionVariable(&instance->spotify_cond);

			LeaveCriticalSection(&instance->spotify_lock);
		}
		break;

		case SPOTIFY_START_NEXT_DOWNLOAD:
		{
			//verifico tipo
			if ( !instance->actual_download_item )
			{
				printf("Download terminati!!\r\n");
				return 0;
			}

			sp_linktype type = sp_link_type(instance->actual_download_item->spotify_link);

			if ( type == SP_LINKTYPE_ALBUM )
			{
				//avvio download album
				PostMessage(instance->spotify_window, SPOTIFY_BROWSE_ALBUM, 0, 0);

				return 0;
			}
			else if ( type == SP_LINKTYPE_TRACK )
			{
				//avvio download traccia
				instance->actual_download_track_is_single = 1;

				PostMessage(instance->spotify_window, SPOTIFY_DOWNLOAD_SINGLE_TRACK, 0, 0);

				return 0;
			}
			else
			{
				printf("Tipo download non valido!\r\n");

				instance->actual_download_item = instance->actual_download_item->next_item;

				PostMessage(instance->spotify_window, SPOTIFY_START_NEXT_DOWNLOAD, 0, 0);

				return 0;
			}


		}
		break;
		

		/*case SPOTIFY_BROWSE_ALBUM:
		{
			//scarico l'album wparam

			if ( instance && instance->search )
			{

				int n_albums = sp_search_num_albums(instance->search);

				if ( instance->actual_download_album >= n_albums )
				{
					printf("Tutti gli album sono stati scaricati!!!\r\n\r\n");
					return 0; //terminato!!
				}

				sp_album *sel_album = sp_search_album(instance->search, instance->actual_download_album);

				if ( sel_album && sp_album_is_loaded(sel_album) )
				{
					EnterCriticalSection(&instance->spotify_lock);

					sp_albumbrowse_create(instance->spotify, sel_album, albumbrowse_complete, instance);

					WakeAllConditionVariable(&instance->spotify_cond);
			
					LeaveCriticalSection(&instance->spotify_lock);
				}
			}
		}
		break;*/

		case SPOTIFY_BROWSE_ALBUM:
		{
			//scarico l'album wparam

			if ( instance && instance->actual_download_item )
			{

				sp_album *sel_album = sp_link_as_album(instance->actual_download_item->spotify_link);

				if ( sel_album )
				{
					EnterCriticalSection(&instance->spotify_lock);

					sp_albumbrowse_create(instance->spotify, sel_album, albumbrowse_complete, instance);

					WakeAllConditionVariable(&instance->spotify_cond);
			
					LeaveCriticalSection(&instance->spotify_lock);
				}
			}
		}
		break;

		case SPOTIFY_DOWNLOAD_ALBUM:
		{
			//avvio il download dell'album

			if ( instance && instance->album_browse )
			{
				instance->actual_download_track_is_single = 0;

				sp_album *album = sp_albumbrowse_album(instance->album_browse);
				
				const char *album_name = sp_album_name(album);

				instance->actual_download_album_name = album_name;
				instance->actual_download_track = 0;
				instance->track = 0;

				memset(instance->actual_dir_name, 0, 1023);
				
				_snprintf(instance->actual_dir_name, 1023, "./%s", album_name);

				//pulisco il nome da spazi e :
				char *p = strpbrk (instance->actual_dir_name, " :");

				while ( p )
				{
					*p = '.';
					
					p = strpbrk(instance->actual_dir_name, " :");
				}

				//creo la directory per l'album

				if ( !CreateDirectoryA(instance->actual_dir_name, NULL) ) 
				{
					if (GetLastError() == 183)
					{
						printf("Album gia' scaricato, skip\r\n\r\n");

						//skip
						instance->actual_download_album = 0;
						instance->actual_download_track = 0;

						instance->actual_download_item = instance->actual_download_item->next_item;

						PostMessage(instance->spotify_window, SPOTIFY_START_NEXT_DOWNLOAD, 0, 0);

					}
					else
					{
						printf("Errore [%d] creazione directory: %s\r\n", GetLastError(), instance->actual_dir_name);
					}

					return 0;
				}

				printf("Avvio download album: %s\r\n", album_name);
				printf("---------------------------------------------------------------\r\n");

				PostMessage(instance->spotify_window, SPOTIFY_DOWNLOAD_TRACK, 0, 0);

			}

		}
		break;

		/*case SPOTIFY_DOWNLOAD_ALBUM:
		{
			//avvio il download dell'album

			if ( instance && instance->album_browse )
			{
				sp_album *album = sp_albumbrowse_album(instance->album_browse);
				
				const char *album_name = sp_album_name(album);

				instance->actual_download_album_name = album_name;
				instance->actual_download_track = 0;
				instance->track = 0;

				memset(instance->actual_dir_name, 0, 1023);
				
				_snprintf(instance->actual_dir_name, 1023, "./%s", album_name);

				//pulisco il nome da spazi e :
				char *p = strpbrk (instance->actual_dir_name, " :");

				while ( p )
				{
					*p = '.';
					
					p = strpbrk(instance->actual_dir_name, " :");
				}

				//creo la directory per l'album

				if ( !CreateDirectoryA(instance->actual_dir_name, NULL) ) 
				{
					if (GetLastError() == 183)
					{
						printf("Album gia' scaricato, skip\r\n\r\n");

						//skip
						instance->actual_download_album++;
						instance->actual_download_track = 0;

						PostMessage(instance->spotify_window, SPOTIFY_BROWSE_ALBUM, 0, 0);

					}
					else
					{
						printf("Errore [%d] creazione directory: %s\r\n", GetLastError(), instance->actual_dir_name);
					}

					return 0;
				}

				printf("Avvio download album: %s\r\n", album_name);
				printf("---------------------------------------------------------------\r\n");

				PostMessage(instance->spotify_window, SPOTIFY_DOWNLOAD_TRACK, 0, 0);

			}

		}
		break;*/

		case SPOTIFY_DOWNLOAD_SINGLE_TRACK:
		{
			if ( instance && instance->actual_download_item )
			{
				//scarico la traccia

				char file_name[2048];
				memset(file_name, 0, 2047);

				instance->track = sp_link_as_track(instance->actual_download_item->spotify_link);

				if ( !instance->track /*|| sp_track_get_availability(instance->spotify, instance->track) != SP_TRACK_AVAILABILITY_AVAILABLE*/ )
				{
					//skip
					instance->track = 0;
					printf("Traccia non disponibile o non valida, skip\r\n");

					instance->actual_download_item = instance->actual_download_item->next_item;

					PostMessage(instance->spotify_window, SPOTIFY_START_NEXT_DOWNLOAD, 0, 0);

				}


				if (sp_track_is_loaded(instance->track))
				{
					//traccia caricata..posso preparare
					const char *track_name = sp_track_name(instance->track);

					_snprintf(file_name, 2047, "%s.mp3", track_name);

					instance->actual_download_track_is_single = 1;
					instance->actual_download_track_name = track_name;
					//instance->actual_file = new WaveFile(file_name);
					instance->actual_samples = 0;
					instance->track_total_samples = sp_track_duration(instance->track);
					instance->last_written_samples = 0;

					instance->fp_mp3 = fopen(file_name, "wb");

					if (!instance->fp_mp3)
					{
						printf("Errore apertura file....\r\n");
						return 0;
					}

					printf("Avvio download %s\r\n", track_name);

				}

				sp_error err = sp_session_player_load(instance->spotify, instance->track);

				if (err == SP_ERROR_OK)
				{
					instance->actual_download_item->start_loading = 0;
					instance->track_downloaded = 0;
					sp_session_player_play(instance->spotify, true);
				}
				else if ( err == SP_ERROR_IS_LOADING )
				{
					//riposto 
					if (!instance->actual_download_item->start_loading)
					{
						printf("Traccia non ancora disponibile...attendere....\r\n");
						instance->actual_download_item->start_loading = 1;
					}

					//posto
					PostMessage(instance->spotify_window, SPOTIFY_DOWNLOAD_SINGLE_TRACK, 0, 0);
				}
				else if (err == SP_ERROR_NO_STREAM_AVAILABLE)
				{
					printf("Impossibile scaricare la traccia \"%s\"...skip!\r\n", sp_track_name(instance->track));

					instance->actual_download_item = instance->actual_download_item->next_item;

					PostMessage(instance->spotify_window, SPOTIFY_START_NEXT_DOWNLOAD, 0, 0);
				}

				return 0;
			}
		}
		break;

		case SPOTIFY_DOWNLOAD_TRACK:
		{
			if ( instance && instance->album_browse )
			{
				//scarico la traccia

				char file_name[2048];
				memset(file_name, 0, 2047);

				int num_track = sp_albumbrowse_num_tracks(instance->album_browse);

				if (instance->actual_download_track >= num_track)  ///debug
				{
					//skip
					instance->actual_download_album++;
					instance->actual_download_track = 0;
					instance->actual_download_track_is_single = 0;

					PostMessage(instance->spotify_window, SPOTIFY_BROWSE_ALBUM, 0, 0);

					return 0;
				}

				instance->track = sp_albumbrowse_track(instance->album_browse, instance->actual_download_track);

				if (!instance->track || sp_track_get_availability(instance->spotify, instance->track) != SP_TRACK_AVAILABILITY_AVAILABLE)
				{
					//skip
					instance->actual_download_track++;
					instance->track = 0;
					printf("Traccia non disponibile o non valida, skip\r\n");

					PostMessage(instance->spotify_window, SPOTIFY_DOWNLOAD_TRACK, 0, 0);

				}

				const char *track_name = sp_track_name(instance->track);

				_snprintf(file_name, 2047, "%s/%03d.%s.mp3", instance->actual_dir_name, instance->actual_download_track + 1, track_name);

				instance->actual_download_track_name = track_name;
				//instance->actual_file = new WaveFile(file_name);
				instance->track_total_samples = 0;
				instance->actual_samples = 0;
				instance->last_written_samples = 0;

				instance->fp_mp3 = fopen(file_name, "wb");

				if ( !instance->fp_mp3 )
				{
					printf("Errore apertura file....\r\n");
					return 0;
				}

				printf("Avvio download %s\r\n", track_name);

				sp_session_player_load(instance->spotify, instance->track);

				sp_session_player_play(instance->spotify, true);

			}
		}
		break;
	}
	return 0L;
}


void __stdcall  logged_in (sp_session *session, sp_error error)
{
	SpotifyUserData *context = (SpotifyUserData *)sp_session_userdata(session);

	EnterCriticalSection(&context->spotify_lock);

	//avvio il caricamento delle playlist
	//sp_playlistcontainer *container = sp_session_playlistcontainer(session);
	
	//context->container = container;

	LeaveCriticalSection(&context->spotify_lock);

	PostMessage(context->spotify_window, SPOTIFY_LOGGED_IN, (WPARAM)error, 0);
}


int SpotifyLogIn ( SpotifyUserData *instance, char *username, char *password )
{
	int ret = 0;
	
	if ( !instance->spotify_logged_in )
	{
		//faccio il logout
		EnterCriticalSection(&instance->spotify_lock);

		if ( username && password )
		{
			sp_error err = sp_session_login(instance->spotify, username, password, 1, 0);

			if ( SP_ERROR_OK != err ) ret = 0;
			else ret = 1;
		}


		LeaveCriticalSection(&instance->spotify_lock);
	}

	return ret;
}

int SpotifyLogOut ( SpotifyUserData *instance )
{
	int ret = 0;
	
	if ( instance->spotify_logged_in )
	{
		//faccio il logout
		EnterCriticalSection(&instance->spotify_lock);

		sp_error err = sp_session_logout(instance->spotify);

		if ( SP_ERROR_OK != err ) ret = 0;
		else ret = 1;

		//disattivo il bottone fino all'effettuato login

		LeaveCriticalSection(&instance->spotify_lock);
	}

	return ret;
}

void __stdcall logged_out (sp_session *session)
{
	SpotifyUserData *context = (SpotifyUserData *)sp_session_userdata(session);

	PostMessage(context->spotify_window, SPOTIFY_LOGGED_OUT, 0, 0);
}

void __stdcall  metadata_updated (sp_session *session)
{
	SpotifyUserData *context = (SpotifyUserData *)sp_session_userdata(session);


	//PostMessage(context->spotify_window, SPOTIFY_METADATA_UPDATED, 0, 0);

}

void __stdcall albumbrowse_complete(sp_albumbrowse *result, void *userdata)
{
	SpotifyUserData *context = (SpotifyUserData *)userdata;

	context->album_browse = result;
	
	PostMessage(context->spotify_window, SPOTIFY_DOWNLOAD_ALBUM, 0, 0);
}


void __stdcall  connection_error (sp_session *session, sp_error error)
{

}

void __stdcall message_to_user (sp_session *session, const char *message)
{

}

void __stdcall notify_main_thread (sp_session *session)
{
	SpotifyUserData *context = (SpotifyUserData *)sp_session_userdata(session);

	EnterCriticalSection(&context->spotify_lock);

	context->spotify_flag = 1;

	WakeAllConditionVariable(&context->spotify_cond);

	LeaveCriticalSection(&context->spotify_lock);
}

/*int __stdcall music_delivery (sp_session *session, const sp_audioformat *format, const void *frames, int num_frames)
{
	SpotifyUserData *context = (SpotifyUserData *)sp_session_userdata(session);

	int ret_frames = 0;

	EnterCriticalSection(&context->spotify_buffer_lock);
	
	if ( context->actual_file && !context->actual_file->HeaderPrepared() )
	{
		context->total_samples = 0;
		context->actual_file->PrepareHeader(format->channels, format->sample_rate);
	}

	//scrivo i campioni
	if ( context->actual_file && context->actual_file->FileReady() )
	{
		ret_frames = context->actual_file->AddSamples((void *)frames, num_frames);

		context->total_samples += ret_frames;

		//float secs = (float)context->total_samples / format->sample_rate;

		//printf("Downloading Album[%i] | %s [%2.2f]\r\n", context->actual_download_album, context->actual_download_track_name, secs);
	}

	WakeAllConditionVariable(&context->spotify_buffer_cond);

	LeaveCriticalSection(&context->spotify_buffer_lock);

	return ret_frames;
}*/

int PrepareEncoder ( SpotifyUserData *context, const sp_audioformat *fmt )
{
	if ( !context || !context->lame) return 0;

	//context->lame = lame_init();

	//if ( !context->lame ) return 0;

	lame_set_num_channels(context->lame, fmt->channels);
	lame_set_in_samplerate(context->lame, fmt->sample_rate); // 
	lame_set_brate(context->lame, 320); //320kbps
	lame_set_mode(context->lame, STEREO); //stereo
	lame_set_quality(context->lame, 2); //alta qualita

	context->track_total_samples = (int)ceil(((double)context->track_total_samples * fmt->sample_rate) / 1000.0);

	if ( lame_init_params(context->lame) < 0 ) 
	{
		//errore
		printf("Errore inizializzazione parametri LAME....\r\n");
		lame_close(context->lame);
		context->lame = 0;
		return 0;
	}

	context->encoder_ready = 1;

	return 1;
}

int AllocMp3Buffer ( SpotifyUserData *context, int num_frames )
{
	//if ( !context->lame ) return 0;

	
	//context->mp3_buffersize = 2 * num_frames + 7200; //non lo uso ..... mi tengo molto largo

	context->mp3_buffersize = 500000;

	context->encoder_buffer = malloc(context->mp3_buffersize);

	memset(context->encoder_buffer, 0, context->mp3_buffersize);
	

	return 1;
}

int FreeMP3Buffer ( SpotifyUserData *context )
{
	if ( context->encoder_buffer ) free(context->encoder_buffer);

	context->encoder_buffer = 0;
	context->mp3_buffersize = 0;

	return 0;
}

int CloseEncoder ( SpotifyUserData *context )
{
	if ( !context ) return 0;
	
	lame_close(context->lame);
	context->lame = 0;

	FreeMP3Buffer(context);
}


int __stdcall music_delivery (sp_session *session, const sp_audioformat *format, const void *frames, int num_frames)
{


	SpotifyUserData *context = (SpotifyUserData *)sp_session_userdata(session);

	int ret_frames = 0;

	if ( context && !context->encoder_ready )
	{
		EnterCriticalSection(&context->spotify_buffer_lock);
		//preparo l'encoder
		if ( !PrepareEncoder(context, format) ) exit(1);
		//configuro il buffer
		//if ( !AllocMp3Buffer(context, num_frames) ) exit(1);

		//primo frame ritardo
		WakeAllConditionVariable(&context->spotify_buffer_cond);

		LeaveCriticalSection(&context->spotify_buffer_lock);

		return 0;
	}

	EnterCriticalSection(&context->spotify_lock);

	if (!context->track_downloaded)
	{
		if (context->actual_samples >= context->track_total_samples)
		{

			printf("\r\nTraccia terminata (autoclose)!!\r\n");

			context->track_downloaded = 1;

			//sp_session_player_play(context->spotify, false);

			//track_ended(context);
			WakeAllConditionVariable(&context->spotify_cond);

			LeaveCriticalSection(&context->spotify_lock);

			PostMessage(context->spotify_window, SPOTIFY_CLOSE_SINGLE_TRACK, 0, 0);

			return 0;
		}
	}
	else
	{
		WakeAllConditionVariable(&context->spotify_cond);

		LeaveCriticalSection(&context->spotify_lock);

		printf("Prossima traccia in caricamento....\r\n");

		return 0;
	}
	
	WakeAllConditionVariable(&context->spotify_cond);

	LeaveCriticalSection(&context->spotify_lock);




	EnterCriticalSection(&context->spotify_buffer_lock);

	//encoding, uso il worst case buffer lenght => 1.25 * n_sample + 7200

	int ret = lame_encode_buffer_interleaved(context->lame, (short *)frames, num_frames, (unsigned char *)context->encoder_buffer, 2 * num_frames + 7200);

	if ( ret > 0 )
	{
		//ok caricato il frame ed encodato, scrivo su file
		if ( context->fp_mp3 ) fwrite(context->encoder_buffer, 1, ret, context->fp_mp3);

		ret_frames = num_frames; //ho comunque consumato tutto, buffering interno

		context->actual_samples += ret_frames;

		float secs = (float)context->actual_samples / format->sample_rate;
		float last_secs = (float)context->last_written_samples / format->sample_rate;
		
		//printf("Downloading Album[%i] | %s [%2.2f]\r\n", context->actual_download_album, context->actual_download_track_name, secs);
		if ( secs - last_secs >= 10.0 )
		{
			context->last_written_samples = context->actual_samples;
			printf("*");
		}
	}
	else
	{

		if ( !ret )
		{
			printf("_");
			ret_frames = num_frames;
		}
		else
		{
			printf("Encoder ERRROR[%d]\r\n", ret);

			ret_frames = 0;

		}
	}

	WakeAllConditionVariable(&context->spotify_buffer_cond);

	LeaveCriticalSection(&context->spotify_buffer_lock);

	return ret_frames;
}


void __stdcall play_token_lost (sp_session *session)
{

}

void __stdcall log_message (sp_session *session, const char *data)
{
	//OutputDebugStringA(data);
	SpotifyUserData *context = (SpotifyUserData *)sp_session_userdata(session);

	if ( context && data )
	{
		int len = strnlen(data, 200);

		if ( len > 0 )
		{
			char *str_msg = (char *)malloc(len + 1);
			memset(str_msg, 0, len+1);
			memcpy(str_msg, data, len);

			PostMessage(context->spotify_window, SPOTIFY_LOG_MESSAGE, 0, (LPARAM)str_msg);
		}
	}
}

void __stdcall search_complete(sp_search *result, void *userdata)
{
	//SpotifyUserData *context = (SpotifyUserData *)userdata;
	
	DownloadItem *item = (DownloadItem *)userdata;

	item->spotify_search = result;
	SpotifyUserData *context = (SpotifyUserData *)item->spotify_userdata;
	
	EnterCriticalSection(&context->spotify_lock);

	char tmp_link[1000];
	memset(tmp_link, 0, 1000);

	sp_link_as_string(item->spotify_link, tmp_link, 999);

	printf("Ricerca %s completata...\r\n", tmp_link);

	WakeAllConditionVariable(&context->spotify_cond);

	LeaveCriticalSection(&context->spotify_lock);

	PostMessage(context->spotify_window, SPOTIFY_SEARCH_ITEM_COMPLETE, 0, (LPARAM)item);

}

/*void __stdcall end_of_track (sp_session *session)
{
	SpotifyUserData *context = (SpotifyUserData *)sp_session_userdata(session);

	//traccia terminata, vado avant

	if ( context )
	{

		printf("Traccia %s terminata!!\r\n", context->actual_download_track_name);

		//azzero variabili di stato

		context->actual_file->CloseFile();

		context->total_samples = 0;
		
		context->actual_download_track_name = 0;

		//incremento indice di traccia e posto
		context->actual_download_track++;

		PostMessage(context->spotify_window, SPOTIFY_DOWNLOAD_TRACK, 0, 0);

	}
}*/

void __stdcall end_of_track (sp_session *session)
{
	SpotifyUserData *context = (SpotifyUserData *)sp_session_userdata(session);

	//traccia terminata, vado avant

	track_ended(context);
}


void track_ended(SpotifyUserData *context)
{
	if (context && context->encoder_buffer && context->track)
	{

		printf("\r\nTraccia %s terminata!!\r\n\r\n", context->actual_download_track_name);

		//flush dei buffer dell'encoder

		int ret = lame_encode_flush(context->lame, (unsigned char *)context->encoder_buffer, context->mp3_buffersize);

		if (ret > 0 && context->fp_mp3)
		{
			//scrivo l'ultimo frame

			fwrite(context->encoder_buffer, 1, ret, context->fp_mp3);
		}

		//chiudo l'encoder ed il file

		//CloseEncoder(context);

		if (context->fp_mp3) fclose(context->fp_mp3);

		context->fp_mp3 = 0;

		//azzero variabili di stato

		context->actual_samples = 0;
		context->track_downloaded = 0;

		context->actual_download_track_name = 0;

		//incremento indice di traccia e posto
		context->actual_download_track++;

		context->encoder_ready = 0;


		sp_session_player_unload(context->spotify);

		if (context->actual_download_track_is_single)
		{
			context->actual_download_item = context->actual_download_item->next_item;
			PostMessage(context->spotify_window, SPOTIFY_START_NEXT_DOWNLOAD, 0, 0);
		}
		else PostMessage(context->spotify_window, SPOTIFY_DOWNLOAD_TRACK, 0, 0);

	}
}

void __stdcall streaming_error (sp_session *session, sp_error error)
{

}

void __stdcall userinfo_updated (sp_session *session)
{

}

void __stdcall start_playback (sp_session *session)
{

}

void __stdcall stop_playback (sp_session *session)
{
	
}

void __stdcall get_audio_buffer_stats (sp_session *session, sp_audio_buffer_stats *stats)
{
	
}

void __stdcall offline_status_updated (sp_session *session)
{

}

void __stdcall offline_error (sp_session *session, sp_error error)
{

}

void __stdcall credentials_blob_updated (sp_session *session, const char *blob)
{

}

void __stdcall connectionstate_updated (sp_session *session)
{

}

void __stdcall scrobble_error (sp_session *session, sp_error error)
{

}

////////////////////////////////////////
void __stdcall playlist_added(sp_playlistcontainer *pc, sp_playlist *playlist, int position, void *userdata)
{

}

void __stdcall playlist_removed(sp_playlistcontainer *pc, sp_playlist *playlist, int position, void *userdata)
{

}

void __stdcall playlist_moved(sp_playlistcontainer *pc, sp_playlist *playlist, int position, int new_position, void *userdata)
{

}

void __stdcall container_loaded(sp_playlistcontainer *pc, void *userdata)
{
	SpotifyUserData *context = (SpotifyUserData *)userdata;

	EnterCriticalSection(&context->spotify_lock);

	if ( sp_playlistcontainer_is_loaded(pc) )
	{
		context->container = pc;

		int num_playlists = sp_playlistcontainer_num_playlists(pc);

		for (int k = 0; k < num_playlists; k++)
		{
			sp_playlist *plist = sp_playlistcontainer_playlist(pc, k);

			sp_playlist_add_callbacks(plist, &playlist_cb, userdata);

			if ( sp_playlist_is_loaded(plist) ) printf("%s\r\n", sp_playlist_name(plist));
		}
	}

	LeaveCriticalSection(&context->spotify_lock);
}


void __stdcall tracks_added(sp_playlist *pl, sp_track *const *tracks, int num_tracks, int position, void *userdata)
{

}

void __stdcall tracks_removed(sp_playlist *pl, const int *tracks, int num_tracks, void *userdata)
{

}

void __stdcall tracks_moved(sp_playlist *pl, const int *tracks, int num_tracks, int new_position, void *userdata)
{

}

void __stdcall playlist_renamed(sp_playlist *pl, void *userdata)
{

}

void __stdcall playlist_state_changed(sp_playlist *pl, void *userdata)
{
	SpotifyUserData *context = (SpotifyUserData *)userdata;

	EnterCriticalSection(&context->spotify_lock);

	if (sp_playlist_is_loaded(pl)) printf("%s\r\n", sp_playlist_name(pl));
		
	LeaveCriticalSection(&context->spotify_lock);

}

void __stdcall playlist_update_in_progress(sp_playlist *pl, bool done, void *userdata)
{

}

void __stdcall playlist_metadata_updated(sp_playlist *pl, void *userdata)
{

}

void __stdcall track_created_changed(sp_playlist *pl, int position, sp_user *user, int when, void *userdata)
{

}

void __stdcall track_seen_changed(sp_playlist *pl, int position, bool seen, void *userdata)
{

}

void __stdcall description_changed(sp_playlist *pl, const char *desc, void *userdata)
{

}

void __stdcall image_changed(sp_playlist *pl, const byte *image, void *userdata)
{

}

void __stdcall track_message_changed(sp_playlist *pl, int position, const char *message, void *userdata)
{

}

void __stdcall subscribers_changed(sp_playlist *pl, void *userdata)
{

}


