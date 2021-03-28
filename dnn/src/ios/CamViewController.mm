/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#import "CamViewController.h"
#import <UIKit/UIKit.h>
#import <MetalKit/MTKTextureLoader.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <floor/floor/floor.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/metal/metal_image.hpp>
#include "dnn_state.hpp"

@class CamViewController;
static CamViewController* cam_view { nullptr };
static image_evaluator img_eval;

@interface CamViewController : UIViewController <UIImagePickerControllerDelegate, UINavigationControllerDelegate>

@property (strong, nonatomic) IBOutlet UIImageView* image_view;
@property (strong, nonatomic) IBOutlet UILabel* image_label;
@property (strong, nonatomic) IBOutlet UIButton* capute_button;
@property (strong, nonatomic) IBOutlet UIView* dnn_progress_super_view;
@property (strong, nonatomic) IBOutlet UIActivityIndicatorView* dnn_progress;
- (IBAction)capture_image:(id)sender;

@end

@implementation CamViewController

@synthesize image_view = _image_view;
@synthesize image_label = _image_label;
@synthesize capute_button = _capute_button;
@synthesize dnn_progress_super_view = _dnn_progress_super_view;
@synthesize dnn_progress = _dnn_progress;

- (void)viewDidLoad {
	[super viewDidLoad];
	
	if (![UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
		log_error("no camera found");
		[self.image_label setText:@"no camera found"];
		[self.capute_button setEnabled:NO];
	}
}

- (IBAction)capture_image:(id)sender {
	UIImagePickerController *picker = [[UIImagePickerController alloc] init];
	picker.delegate = self;
	picker.allowsEditing = YES;
	picker.sourceType = UIImagePickerControllerSourceTypeCamera;
	[self presentViewController:picker animated:YES completion:nullptr];
}

- (void)imagePickerController:(UIImagePickerController *)picker didFinishPickingMediaWithInfo:(NSDictionary *)info {
	UIImage *chosen_image = info[UIImagePickerControllerEditedImage];
	self.image_view.image = chosen_image;
	[picker dismissViewControllerAnimated:YES completion:nullptr];
	
	// signal that we're processing a new image
	[self.capute_button setEnabled:NO];
	[self.image_label setText:@"..."];
	[self.dnn_progress startAnimating];
	[self.dnn_progress_super_view bringSubviewToFront:self.dnn_progress];
	[[self view] bringSubviewToFront:self.dnn_progress_super_view];
	
	// run DNN async
	dispatch_async(dispatch_get_main_queue(), ^{
		// get UIImage as CGImage, convert to MTLTexture, wrap in compute_image, run the DNN
		auto cg_image = [self.image_view.image CGImage];
		string img_label;
		if (cg_image != nullptr) {
			MTKTextureLoader* tex_loader = [[MTKTextureLoader alloc] initWithDevice:((const metal_device&)*dnn_state.dev).device];
			id <MTLTexture> tex = [tex_loader newTextureWithCGImage:cg_image
															options:nullptr
															  error:nullptr];
			auto img = make_shared<metal_image>(*dnn_state.dev_queue, tex);
			log_debug("got camera image of type: $",
					  compute_image::image_type_to_string(img->get_image_type()));
			img_label = img_eval(img);
		} else {
			img_label = "<invalid-image>";
		}
		
		// label voodoo to get proper multi-line text
		self.image_label.numberOfLines = 0;
		[self.image_label setText:[NSString stringWithUTF8String:img_label.c_str()]];
		[self.image_label setEnabled:YES];
		CGSize label_size = [self.image_label.text sizeWithAttributes:@{NSFontAttributeName:self.image_label.font}];
		self.image_label.frame = CGRectMake(self.image_label.frame.origin.x, self.image_label.frame.origin.y,
											self.image_label.frame.size.width, label_size.height);
		
		// and we're done here
		[self.dnn_progress stopAnimating];
		[self.capute_button setEnabled:YES];
	});
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController *)picker {
	[picker dismissViewControllerAnimated:YES completion:nullptr];
}

@end

bool init_cam_view(SDL_Window* wnd, image_evaluator img_eval_) {
	img_eval = img_eval_;
	
	// get UIKit info from SDL
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWindowWMInfo(wnd, &info)) {
		log_error("failed to retrieve window info: $", SDL_GetError());
		return false;
	}
	
	// load and set our camera view
	cam_view = [[CamViewController alloc] initWithNibName:@"camera_view" bundle:nil];
	info.info.uikit.window.rootViewController = cam_view;
	[info.info.uikit.window makeKeyAndVisible];
	return true;
}
